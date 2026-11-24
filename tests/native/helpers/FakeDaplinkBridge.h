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
// `nackAfterReadSector` models the real bridge right after READ_SECTOR:
// it starts sending the 256-byte sector to whatever read comes next, and
// NACKs the following transactions until that transfer is over.
class FakeDaplinkBridge : public I2CDevice {
   public:
    static constexpr uint8_t WHO_AM_I = 0x01;
    static constexpr uint8_t READ_SECTOR = 0x20;
    static constexpr uint8_t WRITE_CONFIG = 0x30;
    static constexpr uint8_t READ_CONFIG = 0x31;
    static constexpr uint8_t CLEAR_CONFIG = 0x32;
    static constexpr uint8_t STATUS = 0x80;
    static constexpr uint8_t ERROR_STATUS = 0x81;

    static constexpr uint8_t STATUS_BUSY = 0x80;
    static constexpr uint8_t ERROR_BAD_PARAM = 0x01;
    static constexpr uint8_t ERROR_CMD_FAILED = 0x80;

    static constexpr size_t ZONE_SIZE = 1024;
    static constexpr size_t READ_CONFIG_LEN = 256;
    static constexpr size_t WRITE_CONFIG_ARGS = 31;
    static constexpr uint32_t TICK_MS = 30;

    FakeDaplinkBridge() : zone(ZONE_SIZE, 0xFF) {}

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

    void store(const char* text, size_t offset = 0) {
        for (size_t i = 0; text[i] != '\0' && offset + i < ZONE_SIZE; ++i) {
            zone[offset + i] = static_cast<uint8_t>(text[i]);
        }
    }

    std::vector<uint8_t> zone;
    uint8_t error = 0;
    bool legacyFirmware = false;
    bool failWrites = false;
    bool hung = false;
    size_t nackAfterReadSector = 0;
    size_t readSectorCommands = 0;
    size_t configCommandsOnLegacy = 0;
    size_t readConfigCommands = 0;
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
            case READ_SECTOR:
                if (legacyFirmware) {
                    error |= ERROR_CMD_FAILED;  // expects a 1-byte sector number
                } else {
                    tx.assign(READ_CONFIG_LEN, 0xFF);
                }
                // TASK_WAIT_FLASH_BUSY: idle again on the following tick.
                waitPending = true;
                waitDue = taskDue + TICK_MS;
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
