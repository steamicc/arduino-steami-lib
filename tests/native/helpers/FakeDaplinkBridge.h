// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Wire.h"

// Model of the STeaMi DAPLink F103 I2C bridge, written after the firmware
// (steamicc/DAPLink, source/board/steami/steami_i2c.c and steami32.c):
//
// - a command runs only once all its argument bytes are received
//   (READ_CONFIG takes 2, WRITE_CONFIG always 31, the others none);
// - a write transaction empties the TX buffer, the command then fills it;
// - every read transaction sends the TX buffer from its first byte, and
//   the master sees 0xFF past its end;
// - READ_CONFIG loads 256 bytes of the 1 KB zone from the given offset,
//   and fails when offset + 256 runs past the zone.
class FakeDaplinkBridge : public I2CDevice {
   public:
    static constexpr uint8_t WHO_AM_I = 0x01;
    static constexpr uint8_t WRITE_CONFIG = 0x30;
    static constexpr uint8_t READ_CONFIG = 0x31;
    static constexpr uint8_t CLEAR_CONFIG = 0x32;
    static constexpr uint8_t STATUS = 0x80;
    static constexpr uint8_t ERROR_STATUS = 0x81;

    static constexpr uint8_t ERROR_BAD_PARAM = 0x01;
    static constexpr uint8_t ERROR_CMD_FAILED = 0x80;

    static constexpr size_t ZONE_SIZE = 1024;
    static constexpr size_t READ_CONFIG_LEN = 256;
    static constexpr size_t WRITE_CONFIG_ARGS = 31;

    FakeDaplinkBridge() : zone(ZONE_SIZE, 0xFF) {}

    uint8_t onWrite(const std::vector<uint8_t>& frame) override {
        tx.clear();
        if (frame.empty()) {
            return 0;  // address probe
        }
        const uint8_t cmd = frame[0];
        const std::vector<uint8_t> args(frame.begin() + 1, frame.end());

        switch (cmd) {
            case WHO_AM_I:
                tx = {0x4C};
                break;
            case STATUS:
                tx = {0x00};
                break;
            case ERROR_STATUS:
                tx = {error};
                break;
            case CLEAR_CONFIG:
                std::fill(zone.begin(), zone.end(), 0xFF);
                break;
            case READ_CONFIG:
                if (args.size() < 2) {
                    ignoredFrames++;  // firmware keeps waiting for the offset
                    break;
                }
                readConfig((static_cast<uint16_t>(args[0]) << 8) | args[1]);
                break;
            case WRITE_CONFIG:
                writeFrameLengths.push_back(frame.size());
                if (args.size() < WRITE_CONFIG_ARGS) {
                    ignoredFrames++;  // firmware keeps waiting for 31 bytes
                    break;
                }
                writeConfig(args);
                break;
            default:
                break;
        }
        return 0;
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
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
    size_t readConfigCommands = 0;
    size_t largestRead = 0;
    size_t ignoredFrames = 0;
    std::vector<size_t> writeFrameLengths;

   private:
    void readConfig(uint16_t offset) {
        readConfigCommands++;
        if (offset + READ_CONFIG_LEN > ZONE_SIZE) {
            error = ERROR_CMD_FAILED;
            return;
        }
        tx.assign(zone.begin() + offset, zone.begin() + offset + READ_CONFIG_LEN);
    }

    void writeConfig(const std::vector<uint8_t>& args) {
        const uint16_t offset = (static_cast<uint16_t>(args[0]) << 8) | args[1];
        const uint8_t length = args[2];
        if (length > args.size() - 3) {
            error = ERROR_BAD_PARAM;
            return;
        }
        if (offset + length > ZONE_SIZE) {
            error = ERROR_CMD_FAILED;
            return;
        }
        std::copy_n(args.begin() + 3, length, zone.begin() + offset);
    }

    std::vector<uint8_t> tx;
};
