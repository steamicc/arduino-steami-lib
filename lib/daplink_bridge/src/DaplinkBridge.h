// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "daplink_bridge_const.h"

class DaplinkBridge {
   public:
    DaplinkBridge(TwoWire& wire = Wire, uint8_t address = DAPLINK_BRIDGE_DEFAULT_ADDR);

    // --- Lifecycle ---
    bool begin();
    uint8_t deviceId();

    // --- Status ---
    // True while the bridge runs a command, or when it does not answer.
    bool busy();

    // --- Config zone (1 KB persistent storage in F103 internal flash) ---
    // DAPLink firmwares older than the config zone (e.g. the 2024
    // release_letssteam builds) lock up when they receive a config command
    // with arguments. Every config method checks hasConfigZone() first and
    // fails without sending anything when the firmware lacks the zone.
    bool hasConfigZone();
    bool clearConfig();
    bool writeConfig(const uint8_t* data, size_t length, uint16_t offset = 0);
    bool writeConfig(const char* str, uint16_t offset = 0);
    size_t readConfig(uint8_t* result, size_t maxLen);

    // --- Raw protocol, for drivers built on the bridge (daplink_flash) ---
    // Wait until the bridge is idle. False after `timeoutMs`.
    bool waitReady(uint32_t timeoutMs);
    // Send [cmd | args...]. The firmware runs a command once it has
    // received all its argument bytes: pass exactly the count it expects
    // (at most DAPLINK_BRIDGE_MAX_ARGS).
    bool sendCommand(uint8_t cmd, const uint8_t* args = nullptr, size_t argLen = 0);
    // Wait `settleMs` for the command just sent to run, then report
    // whether it is known to have succeeded. Send nothing in between: the
    // firmware forgets some failures as soon as it gets another command.
    bool commandSucceeded(uint32_t settleMs, uint32_t timeoutMs);
    // Read the error register, which clears it. False if the bridge did
    // not answer.
    bool readError(uint8_t& error);
    // Read `length` bytes of the answer the last command left in the
    // bridge TX buffer, from its first byte. Returns the count read.
    // Reading STATUS or ERROR replaces that answer: wait with delay()
    // rather than waitReady() before calling this.
    size_t readResponse(uint8_t* buf, size_t length);

   private:
    enum class ConfigZoneSupport : uint8_t { Unknown, Present, Absent };
    enum class CommandResult : uint8_t { Ok, Failed, Unknown };

    TwoWire* _wire;
    uint8_t _address;
    ConfigZoneSupport _configZone = ConfigZoneSupport::Unknown;

    ConfigZoneSupport probeConfigZone();

    // I2C helpers
    bool isPresent();
    bool readReg(uint8_t reg, uint8_t& value);
    bool writeFrame(const uint8_t* data, size_t len);

    // Status / error
    CommandResult commandResult(uint32_t settleMs, uint32_t timeoutMs);
    bool waitNotBusy(uint32_t timeoutMs);
};
