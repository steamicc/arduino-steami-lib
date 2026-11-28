// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiBQ27441.h"

#include <Arduino.h>
#include <math.h>

SteamiBQ27441::SteamiBQ27441(TwoWire& wire, uint16_t capacity_mAh, uint8_t address, int gpout_pin)
    : _wire(wire), _capacity_mAh(capacity_mAh), _address(address), _gpout_pin(gpout_pin) {}

bool SteamiBQ27441::begin() {
    // If a GPOUT pin is wired, the gauge may have been left in shutdown
    // (e.g. by a previous powerOff()) and will not ACK on I2C until it's
    // woken via the GPOUT line. Pulse the wake sequence before probing
    // deviceId() so begin() recovers from shutdown instead of returning
    // false. When no GPOUT pin is configured, disableShutdownMode() is a
    // no-op, so the no-wake-pin path keeps its existing behaviour.
    if (_gpout_pin != -1) {
        disableShutdownMode();
        delay(10);
    }

    if (deviceId() != BQ27441_DEVICE_ID) {
        return false;
    }

    // Switch GPOUT back to input so the caller can attach a SOC1 /
    // BAT_LOW interrupt handler. Done after the device-id probe so the
    // wake pulse above isn't fighting the line.
    configureGpoutInput();

    // Inline the powerOn() body so we can propagate setCapacity()'s
    // failure to the caller. Standalone powerOn() stays void to honour
    // the collection API convention; callers that need to detect the
    // initial-config failure go through begin().
    return setCapacity(_capacity_mAh) != 0;
}

void SteamiBQ27441::configureGpoutInput() {
#ifdef Arduino_h
    if (_gpout_pin != -1) {
        pinMode(_gpout_pin, INPUT_PULLUP);
    }
#endif
}

void SteamiBQ27441::configureGpoutOutput() {
#ifdef Arduino_h
    if (_gpout_pin != -1) {
        pinMode(_gpout_pin, OUTPUT);
    }
#endif
}

void SteamiBQ27441::powerOn() {
    // Wake up fuel gauge ic if in shutdown mode
    disableShutdownMode();
    delay(10);
    setCapacity(_capacity_mAh);
}

void SteamiBQ27441::powerOff() {
    // Put fuel gauge ic in shutdown mode by sending shutdown i2c cmd
    enterShutdownMode();
}

void SteamiBQ27441::enableShutdownMode() {
    executeControlWord(BQ27441_CONTROL_SHUTDOWN_ENABLE);
    _shutdown_en = true;
}

void SteamiBQ27441::enterShutdownMode() {
    configureGpoutInput();
    enableShutdownMode();
    executeControlWord(BQ27441_CONTROL_SHUTDOWN);
}

void SteamiBQ27441::disableShutdownMode() {
#ifdef Arduino_h
    if (_gpout_pin != -1) {
        configureGpoutOutput();
        digitalWrite(_gpout_pin, LOW);
        delay(10);
        digitalWrite(_gpout_pin, HIGH);
        delay(10);
        _shutdown_en = false;
    }
#endif
}

bool SteamiBQ27441::isValidDevice() {
    // Checks if device id returned matches bq27441
    return deviceId() == BQ27441_DEVICE_ID;
}

uint16_t SteamiBQ27441::setCapacity(uint16_t capacity) {
    // Write to STATE subclass(82) of BQ27441 extended memory.
    // Offset 0x0A(10)Design capacity is a 2 - byte piece of data - MSB first
    uint8_t cap_msb = capacity >> 8;
    uint8_t cap_lsb = capacity & 0x00FF;
    uint8_t capacity_data[2] = {cap_msb, cap_lsb};

    return writeExtendedData(BQ27441_ID_STATE, 10, capacity_data, 2);
}

int16_t SteamiBQ27441::currentAverage() {
    // Return average current
    int16_t result = current(CurrentMeasureType::AVG);
    return result;
}

uint16_t SteamiBQ27441::capacityFull() {
    // Return full capacity (mAh)
    return capacity(CapacityMeasureType::FULL);
}

uint16_t SteamiBQ27441::capacityRemaining() {
    return capacity(CapacityMeasureType::REMAIN);
}

uint8_t SteamiBQ27441::stateOfCharge() {
    // Return remaining charge %
    return soc(SocMeasureType::FILTERED);
}

uint8_t SteamiBQ27441::stateOfHealth() {
    // Return state of health %
    return soh(SohMeasureType::PERCENT);
}

uint16_t SteamiBQ27441::voltageMv() {
    // Return current voltage
    return readWord(BQ27441_COMMAND_VOLTAGE);
}

int16_t SteamiBQ27441::current(CurrentMeasureType current_measure_type) {
    int16_t result = 0;
    if (current_measure_type == CurrentMeasureType::AVG) {
        result = readWord(BQ27441_COMMAND_AVG_CURRENT);
    }
    if (current_measure_type == CurrentMeasureType::STBY) {
        result = readWord(BQ27441_COMMAND_STDBY_CURRENT);
    }
    if (current_measure_type == CurrentMeasureType::MAX) {
        result = readWord(BQ27441_COMMAND_MAX_CURRENT);
    }

    return result;
}

int16_t SteamiBQ27441::capacity(CapacityMeasureType capacity_measure_type) {
    int16_t result = 0;
    if (capacity_measure_type == CapacityMeasureType::REMAIN) {
        return readWord(BQ27441_COMMAND_REM_CAPACITY);
    }
    if (capacity_measure_type == CapacityMeasureType::FULL) {
        return readWord(BQ27441_COMMAND_FULL_CAPACITY);
    }
    if (capacity_measure_type == CapacityMeasureType::AVAIL) {
        result = readWord(BQ27441_COMMAND_NOM_CAPACITY);
    }
    if (capacity_measure_type == CapacityMeasureType::AVAIL_FULL) {
        result = readWord(BQ27441_COMMAND_AVAIL_CAPACITY);
    }
    if (capacity_measure_type == CapacityMeasureType::REMAIN_F) {
        result = readWord(BQ27441_COMMAND_REM_CAP_FIL);
    }
    if (capacity_measure_type == CapacityMeasureType::REMAIN_UF) {
        result = readWord(BQ27441_COMMAND_REM_CAP_UNFL);
    }
    if (capacity_measure_type == CapacityMeasureType::FULL_F) {
        result = readWord(BQ27441_COMMAND_FULL_CAP_FIL);
    }
    if (capacity_measure_type == CapacityMeasureType::FULL_UF) {
        result = readWord(BQ27441_COMMAND_FULL_CAP_UNFL);
    }
    if (capacity_measure_type == CapacityMeasureType::DESIGN) {
        result = readWord(BQ27441_EXTENDED_CAPACITY);
    }

    return result;
}

int16_t SteamiBQ27441::power() {
    return readWord(BQ27441_COMMAND_AVG_POWER);
}

uint16_t SteamiBQ27441::soc(SocMeasureType soc_measure_type) {
    uint16_t soc_ret = 0;
    if (soc_measure_type == SocMeasureType::FILTERED) {
        soc_ret = readWord(BQ27441_COMMAND_SOC);
    } else if (soc_measure_type == SocMeasureType::UNFILTERED) {
        soc_ret = readWord(BQ27441_COMMAND_SOC_UNFL);
    }

    return soc_ret;
}

uint16_t SteamiBQ27441::soh(SohMeasureType soh_measure_type) {
    uint16_t soh_raw = readWord(BQ27441_COMMAND_SOH);
    uint16_t soh_status = soh_raw >> 8;
    uint16_t soh_percent = soh_raw & 0x00FF;

    if (soh_measure_type == SohMeasureType::PERCENT) {
        return soh_percent;
    }
    return soh_status;
}

uint16_t SteamiBQ27441::readTemperatureDk(TempMeasureType temp_measure_type) {
    if (temp_measure_type == TempMeasureType::BATTERY) {
        return readWord(BQ27441_COMMAND_TEMP);
    }
    if (temp_measure_type == TempMeasureType::INTERNAL_TEMP) {
        return readWord(BQ27441_COMMAND_INT_TEMP);
    }
    return 0;
}

float SteamiBQ27441::temperature(TempMeasureType temp_measure_type) {
    return readTemperatureDk(temp_measure_type) / 10.0 - 273.15;
}

float SteamiBQ27441::temperatureK(TempMeasureType temp_measure_type) {
    return readTemperatureDk(temp_measure_type) / 10.0;
}

uint16_t SteamiBQ27441::temperatureDk(TempMeasureType temp_measure_type) {
    return readTemperatureDk(temp_measure_type);
}

uint16_t SteamiBQ27441::gpoutPolarity() {
    uint16_t op_config_register = opConfig();
    return op_config_register & BQ27441_OPCONFIG_GPIOPOL;
}

bool SteamiBQ27441::setGpoutPolarity(bool active_high) {
    uint16_t old_op_config = opConfig();

    if ((active_high && (old_op_config & BQ27441_OPCONFIG_GPIOPOL)) ||
        (!active_high && !(old_op_config & BQ27441_OPCONFIG_GPIOPOL)))
        return true;
    uint16_t new_op_config = old_op_config;
    if (active_high) {
        new_op_config |= BQ27441_OPCONFIG_GPIOPOL;
    } else {
        new_op_config &= ~(BQ27441_OPCONFIG_GPIOPOL);
    }

    return writeOpConfig(new_op_config);
}

uint16_t SteamiBQ27441::gpoutFunction() {
    uint16_t op_config_register = opConfig();
    return op_config_register & BQ27441_OPCONFIG_BATLOWEN;
}

bool SteamiBQ27441::setGpoutFunction(bool gpout_function) {
    uint16_t old_op_config = opConfig();
    if ((gpout_function && (old_op_config & BQ27441_OPCONFIG_BATLOWEN)) ||
        (!gpout_function && !(old_op_config & BQ27441_OPCONFIG_BATLOWEN))) {
        return true;
    }
    uint16_t new_op_config = old_op_config;
    if (gpout_function) {
        new_op_config |= BQ27441_OPCONFIG_BATLOWEN;
    } else {
        new_op_config &= ~(BQ27441_OPCONFIG_BATLOWEN);
    }
    return writeOpConfig(new_op_config) != 0;
}

uint16_t SteamiBQ27441::soc1SetThreshold() {
    return readExtendedData(BQ27441_ID_DISCHARGE, 0);
}

uint16_t SteamiBQ27441::soc1ClearThreshold() {
    return readExtendedData(BQ27441_ID_DISCHARGE, 1);
}

// Clamp to 0..100 before narrowing, so 300 % writes 100 % (not 44 %) and
// -5 % writes 0 %, like constrain(x, 0, 100) in MicroPython.
static uint8_t percent(int value) {
    return static_cast<uint8_t>(bq27441_detail::clamp<int>(value, 0, 100));
}

bool SteamiBQ27441::setSoc1Thresholds(int set_soc, int clear_soc) {
    uint8_t thresholds[2] = {percent(set_soc), percent(clear_soc)};
    return writeExtendedData(BQ27441_ID_DISCHARGE, 0, thresholds, 2) != 0;
}

uint16_t SteamiBQ27441::socfSetThreshold() {
    return readExtendedData(BQ27441_ID_DISCHARGE, 2);
}

uint16_t SteamiBQ27441::socfClearThreshold() {
    return readExtendedData(BQ27441_ID_DISCHARGE, 3);
}

bool SteamiBQ27441::setSocfThresholds(int set_socf, int clear_socf) {
    uint8_t thresholds[2] = {percent(set_socf), percent(clear_socf)};
    return writeExtendedData(BQ27441_ID_DISCHARGE, 2, thresholds, 2) != 0;
}

bool SteamiBQ27441::socFlag() {
    return (flags() & BQ27441_FLAG_SOC1) != 0;
}

bool SteamiBQ27441::socfFlag() {
    return (flags() & BQ27441_FLAG_SOCF) != 0;
}

uint16_t SteamiBQ27441::sociDelta() {
    return readExtendedData(BQ27441_ID_STATE, 26);
}

bool SteamiBQ27441::setSociDelta(int delta) {
    uint8_t soci = percent(delta);
    return writeExtendedData(BQ27441_ID_STATE, 26, &soci, 1) != 0;
}

bool SteamiBQ27441::pulseGpout() {
    return executeControlWord(BQ27441_CONTROL_PULSE_SOC_INT) != 0;
}

uint16_t SteamiBQ27441::deviceId() {
    return readControlWord(BQ27441_CONTROL_DEVICE_TYPE);
}

uint32_t SteamiBQ27441::TimeMs() {
    return millis();
}

bool SteamiBQ27441::enterConfig(bool user_control) {
    // Refresh the seal flag from the current chip state on every call.
    // Without this reset, a previous enterConfig() that found the gauge
    // sealed would leave _seal_flag stuck at true even after someone
    // unsealed the chip externally — and the matching exitConfig()
    // would then re-seal it against the caller's intent.
    _seal_flag = sealed();
    if (_seal_flag) {
        unseal();
    }

    if (executeControlWord(BQ27441_CONTROL_SET_CFGUPDATE)) {
        uint32_t start_ms = TimeMs();
        bool timeout = false;
        while (!(flags() & BQ27441_FLAG_CFGUPMODE)) {
            delay(1);
            uint32_t elapsed_ms = TimeMs() - start_ms;
            if (elapsed_ms > BQ27441_I2C_TIMEOUT) {
                timeout = true;
                break;
            }
        }
        if (!timeout) {
            // A user session exists only once the gauge is really in
            // config-update mode: after a failure the setters keep
            // entering and leaving the mode themselves.
            if (user_control) {
                _user_config_control = true;
            }
            return true;
        }
    }
    return false;
}

bool SteamiBQ27441::exitConfig(bool resim) {
    // Ends a user session opened by enterConfig(true): the setters manage
    // the configuration mode themselves again.
    _user_config_control = false;
    if (resim) {
        if (softReset()) {
            uint32_t start_ms = TimeMs();
            bool timeout = false;

            // After softReset, CFGUPMODE clears as the gauge leaves
            // config-update mode. enterConfig() waits for it to go high;
            // exitConfig() must wait for it to go low.
            while (flags() & BQ27441_FLAG_CFGUPMODE) {
                delay(1);
                uint32_t elapsed_ms = TimeMs() - start_ms;
                if (elapsed_ms > BQ27441_I2C_TIMEOUT) {
                    timeout = true;
                    break;
                }
            }

            if (!timeout) {
                if (_seal_flag) {
                    seal();
                }
                return true;
            }
        }
        return false;
    }
    return executeControlWord(BQ27441_CONTROL_EXIT_CFGUPDATE);
}

uint16_t SteamiBQ27441::flags() {
    return readWord(BQ27441_COMMAND_FLAGS);
}

uint16_t SteamiBQ27441::controlStatus() {
    return readControlWord(BQ27441_CONTROL_STATUS);
}

bool SteamiBQ27441::sealed() {
    uint16_t stat = controlStatus();
    return stat & BQ27441_STATUS_SS;
}

bool SteamiBQ27441::seal() {
    // SEALED is a write-only control command — the chip doesn't return
    // a meaningful data word in response, so using readControlWord
    // would always treat the 0 reply as failure. executeControlWord
    // returns the success of the I2C frame itself, which is the right
    // signal here.
    return executeControlWord(BQ27441_CONTROL_SEALED) != 0;
}

bool SteamiBQ27441::unseal() {
    // The TI BQ27441 expects the unseal key sent twice in succession.
    // The previous implementation used readControlWord and skipped the
    // second send if the first reply was 0 (legal — the chip doesn't
    // return data here), leaving the gauge sealed. Both keys must
    // always be issued via executeControlWord; the I2C ACK on each
    // frame is what we care about.
    bool ok = executeControlWord(BQ27441_UNSEAL_KEY) != 0;
    ok = (executeControlWord(BQ27441_UNSEAL_KEY) != 0) && ok;
    return ok;
}

uint16_t SteamiBQ27441::opConfig() {
    return readWord(BQ27441_EXTENDED_OPCONFIG);
}

uint16_t SteamiBQ27441::writeOpConfig(uint16_t value) {
    uint8_t op_config_msb = value >> 8;
    uint8_t op_config_lsb = value & 0x00FF;
    uint8_t op_config_data[2] = {op_config_msb, op_config_lsb};

    return writeExtendedData(BQ27441_ID_REGISTERS, 0, op_config_data, 2);
}

bool SteamiBQ27441::reset() {
    return executeControlWord(BQ27441_CONTROL_RESET) != 0;
}

bool SteamiBQ27441::softReset() {
    return executeControlWord(BQ27441_CONTROL_SOFT_RESET) != 0;
}

int16_t SteamiBQ27441::readWord(uint8_t sub_address) {
    uint8_t data[2];
    if (!readReg(sub_address, data, 2)) {
        return 0;
    }
    return (int16_t)(data[0] | (data[1] << 8));
}

uint16_t SteamiBQ27441::readControlWord(uint16_t function) {
    uint16_t sub_command_msb = function >> 8;
    uint16_t sub_command_lsb = function & 0x00FF;
    uint8_t command[2] = {(uint8_t)sub_command_lsb, (uint8_t)sub_command_msb};
    writeReg(0, command, 2);
    uint8_t data[2];
    if (readReg(0, data, 2)) {
        return (data[1] << 8) | data[0];
    }
    return 0;
}

uint16_t SteamiBQ27441::executeControlWord(uint16_t function) {
    uint16_t sub_command_msb = function >> 8;
    uint16_t sub_command_lsb = function & 0x00FF;
    uint8_t command[2] = {(uint8_t)sub_command_lsb, (uint8_t)sub_command_msb};
    return bool(writeReg(0, command, 2));
}

uint16_t SteamiBQ27441::blockDataControl() {
    uint8_t enable_byte = 0x00;
    return writeReg(BQ27441_EXTENDED_CONTROL, &enable_byte, 1);
}

uint16_t SteamiBQ27441::blockDataClass(uint16_t id) {
    uint8_t id_buf[1] = {(uint8_t)id};
    return writeReg(BQ27441_EXTENDED_DATACLASS, id_buf, 1);
}

bool SteamiBQ27441::blockDataOffset(uint16_t offset) {
    uint8_t id_buf[1] = {(uint8_t)offset};
    return writeReg(BQ27441_EXTENDED_DATABLOCK, id_buf, 1);
}

bool SteamiBQ27441::blockDataChecksum(uint8_t& out) {
    return readReg(BQ27441_EXTENDED_CHECKSUM, &out, 1);
}

uint16_t SteamiBQ27441::readBlockData(uint16_t offset) {
    uint16_t address = offset + BQ27441_EXTENDED_BLOCKDATA;
    uint8_t ret[1];
    if (!readReg(address, ret, 1)) {
        return 0;
    }
    return ret[0];
}

uint16_t SteamiBQ27441::writeBlockData(uint16_t offset, uint16_t data) {
    uint16_t address = offset + BQ27441_EXTENDED_BLOCKDATA;
    uint8_t buf[1] = {(uint8_t)data};
    return writeReg(address, buf, 1);
}

uint16_t SteamiBQ27441::writeBlockChecksum(uint8_t csum) {
    return writeReg(BQ27441_EXTENDED_CHECKSUM, &csum, 1);
}

bool SteamiBQ27441::computeBlockChecksum(uint8_t& out) {
    uint8_t data[32];
    if (!readReg(BQ27441_EXTENDED_BLOCKDATA, data, 32)) {
        return false;
    }
    uint16_t csum = 0;
    for (int i = 0; i < 32; i++) {
        csum += data[i];
    }
    out = static_cast<uint8_t>((255 - (csum & 0xFF)) & 0xFF);
    return true;
}

uint16_t SteamiBQ27441::readExtendedData(uint16_t class_id, uint16_t offset) {
    bool entered_config = false;
    if (!_user_config_control) {
        if (!enterConfig(false)) {
            return 0;
        }
        entered_config = true;
    }

    // We own the config-mode lifecycle from this point. Any failure
    // below has to call exitConfig() before returning, otherwise the
    // gauge stays in CFGUPDATE (and possibly unsealed) and the next
    // public read/write sees an inconsistent state machine.
    auto bail = [&](uint16_t result) -> uint16_t {
        if (entered_config) {
            exitConfig();
        }
        return result;
    };

    if (!blockDataControl()) {
        return bail(0);
    }
    if (!blockDataClass(class_id)) {
        return bail(0);
    }

    if (!blockDataOffset(offset / 32)) {
        return bail(0);
    }

    // The gauge requires the staging buffer to be primed (via a read of
    // the 32-byte block) and the on-chip checksum to be fetched before
    // it will honour subsequent BLOCKDATA reads. We don't compare the
    // computed and stored checksums here — that's a write-side concern
    // — but a partial I2C transaction would leave the gauge in a
    // half-prepared state, so propagate the failure as 0.
    uint8_t computed_csum = 0;
    if (!computeBlockChecksum(computed_csum)) {
        return bail(0);
    }
    uint8_t stored_csum = 0;
    if (!blockDataChecksum(stored_csum)) {
        return bail(0);
    }

    uint16_t ret_data = readBlockData(offset % 32);

    if (entered_config) {
        exitConfig();
    }

    return ret_data;
}

uint16_t SteamiBQ27441::writeExtendedData(uint16_t class_id, uint16_t offset, const uint8_t* data,
                                          uint16_t length) {
    // Reject a write that does not fit in one 32-byte block rather than
    // chunking it: callers are expected to issue one writeExtendedData
    // per page. See bq27441_detail::isExtendedDataWriteInRange().
    if (!bq27441_detail::isExtendedDataWriteInRange(offset, length)) {
        return false;
    }

    bool entered_config = false;
    if (!_user_config_control) {
        if (!enterConfig(false)) {
            return false;
        }
        entered_config = true;
    }

    // See readExtendedData() — any failure past enterConfig() has to
    // run exitConfig() before returning so we don't leave the gauge
    // stuck in CFGUPDATE.
    auto bail = [&](uint16_t result) -> uint16_t {
        if (entered_config) {
            exitConfig();
        }
        return result;
    };

    if (!blockDataControl()) {
        return bail(false);
    }

    if (!blockDataClass(class_id)) {
        return bail(false);
    }

    if (!blockDataOffset(offset / 32)) {
        return bail(false);
    }

    // Prime the staging buffer and pull the gauge-side checksum so the
    // block is in a known-good state before we start patching it. We
    // discard both values, but a partial I2C frame here would leave
    // the gauge with a stale staging buffer that would silently corrupt
    // the eventual writeBlockChecksum() below.
    uint8_t primed_csum = 0;
    if (!computeBlockChecksum(primed_csum)) {
        return bail(false);
    }
    uint8_t old_csum = 0;
    if (!blockDataChecksum(old_csum)) {
        return bail(false);
    }

    for (int i = 0; i < length; i++) {
        if (!writeBlockData((offset % 32) + i, data[i])) {
            return bail(false);
        }
    }

    uint8_t new_csum = 0;
    if (!computeBlockChecksum(new_csum)) {
        return bail(false);
    }
    if (!writeBlockChecksum(new_csum)) {
        return bail(false);
    }

    if (entered_config) {
        exitConfig();
    }

    return true;
}

bool SteamiBQ27441::readReg(uint8_t sub_address, uint8_t* buf, uint16_t count) {
    _wire.beginTransmission(_address);
    _wire.write(sub_address);
    if (_wire.endTransmission(false) != 0)
        return false;
    // Short reads return -1 from Wire.read() which becomes 0xFF when
    // assigned to a uint8_t, so the caller would silently parse garbage.
    // Treat any partial response as a hard failure. The static_cast
    // pins the `requestFrom(uint8_t, uint8_t)` overload on the STM32
    // toolchain — passing the uint16_t `count` directly would trigger
    // an ambiguous-overload warning between the (int,int) and
    // (uint8_t,uint8_t) signatures.
    const uint8_t count8 = static_cast<uint8_t>(count);
    if (_wire.requestFrom(_address, count8) != count8) {
        return false;
    }
    for (uint16_t i = 0; i < count; i++) {
        if (!_wire.available()) {
            return false;
        }
        buf[i] = static_cast<uint8_t>(_wire.read());
    }
    return true;
}

bool SteamiBQ27441::writeReg(uint8_t sub_address, const uint8_t* buf, uint16_t count) {
    _wire.beginTransmission(_address);
    _wire.write(sub_address);
    for (uint16_t i = 0; i < count; i++) {
        _wire.write(buf[i]);
    }
    return _wire.endTransmission() == 0;
}

uint16_t SteamiBQ27441::status() {
    return controlStatus();
}

bool SteamiBQ27441::dataReady() {
    return (status() & BQ27441_STATUS_INITCOMP) != 0;
}