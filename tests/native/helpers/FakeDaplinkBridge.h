// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Arduino.h"
#include "Wire.h"

// Model of the STeaMi DAPLink F103 I2C bridge, written after the firmware
// (steamicc/DAPLink, source/board/steami/steami_i2c.c and steami32.c):
//
// - a command runs only once all its argument bytes are received
//   (READ_SECTOR and READ_CONFIG take 2, WRITE_CONFIG always 31, the
//   others none);
// - WHO_AM_I, STATUS and ERROR_STATUS are answered at once; READ_SECTOR,
//   READ_CONFIG, WRITE_CONFIG and CLEAR_CONFIG are tasks that the F103
//   main loop runs on its next 30 ms tick. The bridge reports busy from
//   the command until the task is done; READ_SECTOR then waits one more
//   tick for the external flash;
// - a write transaction empties the TX buffer, a command or a task fills
//   it; every read transaction sends the TX buffer from its first byte,
//   and the master sees 0xFF past its end;
// - READ_CONFIG loads 256 bytes of the 1 KB zone from the given offset,
//   and fails when offset + 256 runs past the zone;
// - reading the error register returns it and clears it. Every other
//   command clears its "last command failed" bit (0x80), so a task
//   failure reported only through that bit is lost as soon as the host
//   sends anything else, a STATUS poll included.
//
// With `legacyFirmware` set, it models the builds that predate the config
// zone (release_letssteam, 2024): config commands are unknown, READ_SECTOR
// rejects its 2-byte sector number, and an unknown command followed by
// argument bytes locks the F103 up (`hung`: every later transaction fails).
//
// The user file lives in `flash` (the external W25Q64, shortened): it runs
// from byte 0 to the first 0xFF. SET_FILENAME (11 bytes) and GET_FILENAME
// handle its 8.3 name, WRITE_DATA appends ([len | data | padding], always
// 31 argument bytes), CLEAR_FLASH erases it and READ_SECTOR returns 256
// bytes of it. These tasks keep the bridge busy one more tick while the
// flash finishes.
//
// `nackAfterReadSector` models the real bridge right after READ_SECTOR:
// it starts sending the 256-byte sector to whatever read comes next, and
// NACKs the following transactions until that transfer is over.
//
// `nackReadConfigFrom` NACKs every READ_CONFIG from that one on (0 is the
// first READ_CONFIG received): an I2C error in the middle of a readConfig().
class FakeDaplinkBridge : public I2CDevice {
   public:
    static constexpr uint8_t WHO_AM_I = 0x01;
    static constexpr uint8_t SET_FILENAME = 0x03;
    static constexpr uint8_t GET_FILENAME = 0x04;
    static constexpr uint8_t CLEAR_FLASH = 0x10;
    static constexpr uint8_t WRITE_DATA = 0x11;
    static constexpr uint8_t READ_SECTOR = 0x20;
    static constexpr uint8_t WRITE_CONFIG = 0x30;
    static constexpr uint8_t READ_CONFIG = 0x31;
    static constexpr uint8_t CLEAR_CONFIG = 0x32;
    static constexpr uint8_t STATUS = 0x80;
    static constexpr uint8_t ERROR_STATUS = 0x81;

    static constexpr uint8_t STATUS_BUSY = 0x80;
    static constexpr uint8_t ERROR_BAD_PARAM = 0x01;
    static constexpr uint8_t ERROR_BAD_FILENAME = 0x02;
    static constexpr uint8_t ERROR_FILE_FULL = 0x04;
    static constexpr uint8_t ERROR_CMD_FAILED = 0x80;

    static constexpr size_t ZONE_SIZE = 1024;
    static constexpr size_t READ_CONFIG_LEN = 256;
    static constexpr size_t WRITE_CONFIG_ARGS = 31;
    static constexpr size_t WRITE_DATA_ARGS = 31;
    static constexpr size_t FILENAME_ARGS = 11;
    static constexpr size_t SECTOR_SIZE = 256;
    static constexpr size_t FLASH_SIZE = 4096;
    static constexpr uint32_t TICK_MS = 30;

    FakeDaplinkBridge() : zone(ZONE_SIZE, 0xFF), flash(FLASH_SIZE, 0xFF), filename(11, 0xFF) {}

    uint8_t onWrite(const std::vector<uint8_t>& frame) override {
        if (hung) {
            return 2;  // NACK: the F103 no longer answers
        }
        runDueTasks();
        if (nackPending > 0) {
            nackPending--;
            silent = true;
            return 3;  // NACK on data: still busy sending the last block
        }
        silent = false;
        tx.clear();
        if (frame.empty()) {
            return 0;  // address probe
        }
        const uint8_t cmd = frame[0];
        const std::vector<uint8_t> args(frame.begin() + 1, frame.end());

        if (legacyFirmware && (cmd == WRITE_CONFIG || cmd == READ_CONFIG || cmd == CLEAR_CONFIG)) {
            configCommandsOnLegacy++;
            if (!args.empty()) {
                hung = true;
            }
            return 0;
        }

        switch (cmd) {
            case WHO_AM_I:
                tx = {0x4C};
                break;
            case STATUS:
                tx = {static_cast<uint8_t>(busy() ? STATUS_BUSY : 0x00)};
                break;
            case ERROR_STATUS:
                tx = {error};
                error = 0;
                return 0;  // already cleared, nothing left to reset
            case READ_SECTOR:
                if (args.size() < 2) {
                    ignoredFrames++;  // firmware keeps waiting for the sector
                    break;
                }
                readSectorCommands++;
                schedule(cmd, args);
                nackPending = nackAfterReadSector;
                break;
            case READ_CONFIG:
                if (args.size() < 2) {
                    ignoredFrames++;  // firmware keeps waiting for the offset
                    break;
                }
                if (readConfigFrames++ >= nackReadConfigFrom) {
                    return 2;  // NACK on address: models an I2C error
                }
                schedule(cmd, args);
                break;
            case WRITE_CONFIG:
                writeFrameLengths.push_back(frame.size());
                if (args.size() < WRITE_CONFIG_ARGS) {
                    ignoredFrames++;  // firmware keeps waiting for 31 bytes
                    break;
                }
                schedule(cmd, args);
                break;
            case CLEAR_CONFIG:
            case GET_FILENAME:
            case CLEAR_FLASH:
                schedule(cmd, args);
                break;
            case SET_FILENAME:
                if (args.size() < FILENAME_ARGS) {
                    ignoredFrames++;  // firmware keeps waiting for 11 bytes
                    break;
                }
                schedule(cmd, args);
                break;
            case WRITE_DATA:
                writeFrameLengths.push_back(frame.size());
                if (args.size() < WRITE_DATA_ARGS) {
                    ignoredFrames++;  // firmware keeps waiting for 31 bytes
                    break;
                }
                schedule(cmd, args);
                break;
            default:
                break;
        }
        // on_I2C_receive_command() ends every command this way.
        error &= static_cast<uint8_t>(~ERROR_CMD_FAILED);
        return 0;
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        if (hung || silent) {
            return {};  // the master gets no byte at all
        }
        runDueTasks();
        largestRead = std::max(largestRead, quantity);
        std::vector<uint8_t> out(quantity, 0xFF);
        std::copy_n(tx.begin(), std::min(quantity, tx.size()), out.begin());
        return out;
    }

    // Current user file: the flash content up to its first 0xFF.
    std::vector<uint8_t> file() const {
        auto end = std::find(flash.begin(), flash.end(), 0xFF);
        return std::vector<uint8_t>(flash.begin(), end);
    }

    void storeFile(const char* text) {
        for (size_t i = 0; text[i] != '\0' && i < FLASH_SIZE; ++i) {
            flash[i] = static_cast<uint8_t>(text[i]);
        }
    }

    void store(const char* text, size_t offset = 0) {
        for (size_t i = 0; text[i] != '\0' && offset + i < ZONE_SIZE; ++i) {
            zone[offset + i] = static_cast<uint8_t>(text[i]);
        }
    }

    std::vector<uint8_t> zone;
    std::vector<uint8_t> flash;
    std::vector<uint8_t> filename;
    uint8_t error = 0;
    size_t clearFlashCommands = 0;
    bool legacyFirmware = false;
    bool failWrites = false;
    bool hung = false;
    size_t nackAfterReadSector = 0;
    size_t readSectorCommands = 0;
    size_t configCommandsOnLegacy = 0;
    size_t readConfigCommands = 0;
    size_t readConfigFrames = 0;
    size_t nackReadConfigFrom = SIZE_MAX;
    size_t largestRead = 0;
    size_t ignoredFrames = 0;
    std::vector<size_t> writeFrameLengths;

   private:
    // Next 30 ms tick of the F103 main loop strictly after `t`.
    static uint32_t nextTick(uint32_t t) { return (t / TICK_MS + 1) * TICK_MS; }

    bool busy() const { return taskPending || waitPending; }

    void schedule(uint8_t cmd, const std::vector<uint8_t>& args) {
        if (busy()) {
            return;  // "ERROR I2C busy.": the firmware drops the command
        }
        taskPending = true;
        taskCmd = cmd;
        taskArgs = args;
        taskDue = nextTick(millis());
    }

    void runDueTasks() {
        const uint32_t now = millis();
        if (taskPending && now >= taskDue) {
            taskPending = false;
            runTask();
        }
        if (waitPending && now >= waitDue) {
            waitPending = false;
        }
    }

    void runTask() {
        switch (taskCmd) {
            case READ_SECTOR: {
                const size_t sector = (static_cast<size_t>(taskArgs[0]) << 8) | taskArgs[1];
                if (legacyFirmware || sector >= FLASH_SIZE / SECTOR_SIZE) {
                    // Legacy builds expect a 1-byte sector number.
                    error |= ERROR_CMD_FAILED;
                } else {
                    tx.assign(flash.begin() + sector * SECTOR_SIZE,
                              flash.begin() + (sector + 1) * SECTOR_SIZE);
                }
                waitFlash();
                break;
            }
            case SET_FILENAME:
                if (!filenameValid(taskArgs)) {
                    error |= ERROR_BAD_FILENAME;
                    break;
                }
                if (failWrites) {
                    error |= ERROR_CMD_FAILED;
                    break;
                }
                filename.assign(taskArgs.begin(), taskArgs.begin() + FILENAME_ARGS);
                waitFlash();
                break;
            case GET_FILENAME:
                tx = filename;
                waitFlash();
                break;
            case CLEAR_FLASH: {
                clearFlashCommands++;
                const size_t size = file().size();
                std::fill(flash.begin(), flash.begin() + size, 0xFF);
                error &= static_cast<uint8_t>(~ERROR_FILE_FULL);
                waitFlash();
                break;
            }
            case WRITE_DATA:
                appendData(taskArgs);
                waitFlash();
                break;
            case READ_CONFIG:
                readConfig((static_cast<uint16_t>(taskArgs[0]) << 8) | taskArgs[1]);
                break;
            case WRITE_CONFIG:
                writeConfig(taskArgs);
                break;
            case CLEAR_CONFIG:
                if (failWrites) {
                    error |= ERROR_CMD_FAILED;
                    break;
                }
                std::fill(zone.begin(), zone.end(), 0xFF);
                break;
            default:
                break;
        }
    }

    // TASK_WAIT_FLASH_BUSY: idle again on the following tick.
    void waitFlash() {
        waitPending = true;
        waitDue = taskDue + TICK_MS;
    }

    // DAPLink filename_valid(): no lower case, no control character, none
    // of "*+,./:;<=>?[\]|, and no space first.
    static bool filenameValid(const std::vector<uint8_t>& name) {
        static const char kInvalid[] = "\"*+,./:;<=>?[\\]|";
        if (name[0] == ' ' || name[0] == 0x00 || name[0] == 0xE5) {
            return false;
        }
        for (size_t i = 0; i < FILENAME_ARGS; ++i) {
            const uint8_t c = name[i];
            if ((c >= 'a' && c <= 'z') || c < 0x20) {
                return false;
            }
            for (const char* p = kInvalid; *p != '\0'; ++p) {
                if (c == static_cast<uint8_t>(*p)) {
                    return false;
                }
            }
        }
        return true;
    }

    void appendData(const std::vector<uint8_t>& args) {
        if (failWrites) {
            error |= ERROR_CMD_FAILED;
            return;
        }
        size_t size = file().size();
        if (size == FLASH_SIZE) {
            error |= ERROR_FILE_FULL;
            return;
        }
        const size_t length = std::min<size_t>(args[0], args.size() - 1);
        for (size_t i = 0; i < length && size < FLASH_SIZE; ++i) {
            flash[size++] = args[1 + i];
        }
    }

    void readConfig(uint16_t offset) {
        readConfigCommands++;
        if (offset + READ_CONFIG_LEN > ZONE_SIZE) {
            error |= ERROR_CMD_FAILED;
            return;
        }
        tx.assign(zone.begin() + offset, zone.begin() + offset + READ_CONFIG_LEN);
    }

    void writeConfig(const std::vector<uint8_t>& args) {
        const uint16_t offset = (static_cast<uint16_t>(args[0]) << 8) | args[1];
        const uint8_t length = args[2];
        if (length > args.size() - 3) {
            error |= ERROR_BAD_PARAM;
            return;
        }
        if (failWrites || offset + length > ZONE_SIZE) {
            error |= ERROR_CMD_FAILED;
            return;
        }
        std::copy_n(args.begin() + 3, length, zone.begin() + offset);
    }

    std::vector<uint8_t> tx;
    size_t nackPending = 0;
    bool silent = false;
    bool taskPending = false;
    uint8_t taskCmd = 0;
    std::vector<uint8_t> taskArgs;
    uint32_t taskDue = 0;
    bool waitPending = false;
    uint32_t waitDue = 0;
};
