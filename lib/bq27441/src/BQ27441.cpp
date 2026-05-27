// SPDX-License-Identifier: GPL-3.0-or-later

#include "BQ27441.h"

#include <Arduino.h>
#include <math.h>

BQ27441::BQ27441(TwoWire& wire, uint16_t capacity_mAh, uint8_t address, int gpout_pin)
    : _wire(wire), _capacity_mAh(capacity_mAh), _address(address), _gpout_pin(gpout_pin) {}

bool BQ27441::begin() {
    if (deviceId() != BQ27441_DEVICE_ID) {
        return false;
    }

    configureGpoutInput();

    // Inline the powerOn() body so we can propagate setCapacity()'s
    // failure to the caller. Standalone powerOn() stays void to honour
    // the collection API convention; callers that need to detect the
    // initial-config failure go through begin().
    disableShutdownMode();
    delay(10);
    return setCapacity(_capacity_mAh) != 0;
}

void BQ27441::configureGpoutInput() {
#ifdef Arduino_h
    if (_gpout_pin != -1) {
        pinMode(_gpout_pin, INPUT_PULLUP);
    }
#endif
}

void BQ27441::configureGpoutOutput() {
#ifdef Arduino_h
    if (_gpout_pin != -1) {
        pinMode(_gpout_pin, OUTPUT);
    }
#endif
}

void BQ27441::powerOn() {
    // Wake up fuel gauge ic if in shutdown mode
    disableShutdownMode();
    delay(10);
    setCapacity(_capacity_mAh);
}

void BQ27441::powerOff() {
    // Put fuel gauge ic in shutdown mode by sending shutdown i2c cmd
    enterShutdownMode();
}

void BQ27441::enableShutdownMode() {
    executeControlWord(BQ27441_CONTROL_SHUTDOWN_ENABLE);
    _shutdown_en = true;
}

void BQ27441::enterShutdownMode() {
    configureGpoutInput();
    enableShutdownMode();
    executeControlWord(BQ27441_CONTROL_SHUTDOWN);
}

void BQ27441::disableShutdownMode() {
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

bool BQ27441::isValidDevice() {
    // Checks if device id returned matches bq27441
    return deviceId() == BQ27441_DEVICE_ID;
}

uint16_t BQ27441::setCapacity(uint16_t capacity) {
    // Write to STATE subclass(82) of BQ27441 extended memory.
    // Offset 0x0A(10)Design capacity is a 2 - byte piece of data - MSB first
    uint8_t cap_msb = capacity >> 8;
    uint8_t cap_lsb = capacity & 0x00FF;
    uint8_t capacity_data[2] = {cap_msb, cap_lsb};

    return writeExtendedData(BQ27441_ID_STATE, 10, capacity_data, 2);
}

int16_t BQ27441::currentAverage() {
    // Return average current
    int16_t result = current(CurrentMeasureType::AVG);
    return result;
}

uint16_t BQ27441::capacityFull() {
    // Return full capacity (mAh)
    return capacity(CapacityMeasureType::FULL);
}

uint16_t BQ27441::capacityRemaining() {
    return capacity(CapacityMeasureType::REMAIN);
}

uint8_t BQ27441::stateOfCharge() {
    // Return remaining charge %
    return soc(SocMeasureType::FILTERED);
}

uint8_t BQ27441::stateOfHealth() {
    // Return state of health %
    return soh(SohMeasureType::PERCENT);
}

uint16_t BQ27441::voltageMv() {
    // Return current voltage
    return readWord(BQ27441_COMMAND_VOLTAGE);
}

int16_t BQ27441::current(CurrentMeasureType current_measure_type) {
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

int16_t BQ27441::capacity(CapacityMeasureType capacity_measure_type) {
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

int16_t BQ27441::power() {
    return readWord(BQ27441_COMMAND_AVG_POWER);
}

uint16_t BQ27441::soc(SocMeasureType soc_measure_type) {
    uint16_t soc_ret = 0;
    if (soc_measure_type == SocMeasureType::FILTERED) {
        soc_ret = readWord(BQ27441_COMMAND_SOC);
    } else if (soc_measure_type == SocMeasureType::UNFILTERED) {
        soc_ret = readWord(BQ27441_COMMAND_SOC_UNFL);
    }

    return soc_ret;
}

uint16_t BQ27441::soh(SohMeasureType soh_measure_type) {
    uint16_t soh_raw = readWord(BQ27441_COMMAND_SOH);
    uint16_t soh_status = soh_raw >> 8;
    uint16_t soh_percent = soh_raw & 0x00FF;

    if (soh_measure_type == SohMeasureType::PERCENT) {
        return soh_percent;
    }
    return soh_status;
}

uint16_t BQ27441::readTemperatureDk(TempMeasureType temp_measure_type) {
    if (temp_measure_type == TempMeasureType::BATTERY) {
        return readWord(BQ27441_COMMAND_TEMP);
    }
    if (temp_measure_type == TempMeasureType::INTERNAL_TEMP) {
        return readWord(BQ27441_COMMAND_INT_TEMP);
    }
    return 0;
}

float BQ27441::temperature(TempMeasureType temp_measure_type) {
    return readTemperatureDk(temp_measure_type) / 10.0 - 273.15;
}

float BQ27441::temperatureK(TempMeasureType temp_measure_type) {
    return readTemperatureDk(temp_measure_type) / 10.0;
}

uint16_t BQ27441::temperatureDk(TempMeasureType temp_measure_type) {
    return readTemperatureDk(temp_measure_type);
}

uint16_t BQ27441::gpoutPolarity() {
    uint16_t op_config_register = opConfig();
    return op_config_register & BQ27441_OPCONFIG_GPIOPOL;
}

bool BQ27441::setGpoutPolarity(bool active_high) {
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

uint16_t BQ27441::gpoutFunction() {
    uint16_t op_config_register = opConfig();
    return op_config_register & BQ27441_OPCONFIG_BATLOWEN;
}

bool BQ27441::setGpoutFunction(bool gpout_function) {
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
    return writeOpConfig(new_op_config);
}

uint16_t BQ27441::soc1SetThreshold() {
    return readExtendedData(BQ27441_ID_DISCHARGE, 0);
}

uint16_t BQ27441::setSoc1Thresholds(uint16_t set_soc, uint16_t clear_soc) {
    uint8_t thresholds[2] = {0, 0};
    thresholds[0] = bq27441_detail::clamp<uint8_t>(static_cast<uint8_t>(set_soc), 0, 100);
    thresholds[1] = bq27441_detail::clamp<uint8_t>(static_cast<uint8_t>(clear_soc), 0, 100);
    return writeExtendedData(BQ27441_ID_DISCHARGE, 0, thresholds, 2);
}

uint16_t BQ27441::socfSetThreshold() {
    return readExtendedData(BQ27441_ID_DISCHARGE, 2);
}

uint16_t BQ27441::socfClearThreshold() {
    return readExtendedData(BQ27441_ID_DISCHARGE, 3);
}

uint16_t BQ27441::setSocfThresholds(uint16_t set_socf, uint16_t clear_socf) {
    uint8_t thresholds[2] = {0, 0};
    thresholds[0] = bq27441_detail::clamp<uint8_t>(static_cast<uint8_t>(set_socf), 0, 100);
    thresholds[1] = bq27441_detail::clamp<uint8_t>(static_cast<uint8_t>(clear_socf), 0, 100);
    return writeExtendedData(BQ27441_ID_DISCHARGE, 2, thresholds, 2);
}

uint16_t BQ27441::socFlag() {
    uint8_t flag_state = flags();
    return flag_state & BQ27441_FLAG_SOC1;
}

uint16_t BQ27441::sociDelta() {
    return readExtendedData(BQ27441_ID_STATE, 26);
}

uint16_t BQ27441::setSociDelta(uint16_t delta) {
    uint8_t soci = bq27441_detail::clamp<uint8_t>(static_cast<uint8_t>(delta), 0, 100);
    return writeExtendedData(BQ27441_ID_STATE, 26, &soci, 1);
}

uint16_t BQ27441::pulseGpout() {
    return executeControlWord(BQ27441_CONTROL_PULSE_SOC_INT);
}

uint16_t BQ27441::deviceId() {
    return readControlWord(BQ27441_CONTROL_DEVICE_TYPE);
}

uint32_t BQ27441::TimeMs() {
    return millis();
}

bool BQ27441::enterConfig(bool user_control) {
    if (user_control) {
        _user_config_control = true;
    }

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
            return true;
        }
    }
    return false;
}

bool BQ27441::exitConfig(bool resim) {
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

uint16_t BQ27441::flags() {
    return readWord(BQ27441_COMMAND_FLAGS);
}

uint16_t BQ27441::controlStatus() {
    return readControlWord(BQ27441_CONTROL_STATUS);
}

bool BQ27441::sealed() {
    uint16_t stat = controlStatus();
    return stat & BQ27441_STATUS_SS;
}

bool BQ27441::seal() {
    // SEALED is a write-only control command — the chip doesn't return
    // a meaningful data word in response, so using readControlWord
    // would always treat the 0 reply as failure. executeControlWord
    // returns the success of the I2C frame itself, which is the right
    // signal here.
    return executeControlWord(BQ27441_CONTROL_SEALED) != 0;
}

bool BQ27441::unseal() {
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

uint16_t BQ27441::opConfig() {
    return readWord(BQ27441_EXTENDED_OPCONFIG);
}

uint16_t BQ27441::writeOpConfig(uint16_t value) {
    uint8_t op_config_msb = value >> 8;
    uint8_t op_config_lsb = value & 0x00FF;
    uint8_t op_config_data[2] = {op_config_msb, op_config_lsb};

    return writeExtendedData(BQ27441_ID_REGISTERS, 0, op_config_data, 2);
}

uint16_t BQ27441::reset() {
    return executeControlWord(BQ27441_CONTROL_RESET);
}

uint16_t BQ27441::softReset() {
    return executeControlWord(BQ27441_CONTROL_SOFT_RESET);
}

int16_t BQ27441::readWord(uint8_t sub_address) {
    uint8_t data[2];
    if (!readReg(sub_address, data, 2)) {
        return 0;
    }
    return (int16_t)(data[0] | (data[1] << 8));
}

uint16_t BQ27441::readControlWord(uint16_t function) {
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

uint16_t BQ27441::executeControlWord(uint16_t function) {
    uint16_t sub_command_msb = function >> 8;
    uint16_t sub_command_lsb = function & 0x00FF;
    uint8_t command[2] = {(uint8_t)sub_command_lsb, (uint8_t)sub_command_msb};
    return bool(writeReg(0, command, 2));
}

uint16_t BQ27441::blockDataControl() {
    uint8_t enable_byte = 0x00;
    return writeReg(BQ27441_EXTENDED_CONTROL, &enable_byte, 1);
}

uint16_t BQ27441::blockDataClass(uint16_t id) {
    uint8_t id_buf[1] = {(uint8_t)id};
    return writeReg(BQ27441_EXTENDED_DATACLASS, id_buf, 1);
}

uint16_t BQ27441::blockDataOffset(uint16_t offset) {
    uint8_t id_buf[1] = {(uint8_t)offset};
    return writeReg(BQ27441_EXTENDED_DATABLOCK, id_buf, 1);
}

uint16_t BQ27441::blockDataChecksum() {
    uint8_t csum[1];
    if (!readReg(BQ27441_EXTENDED_CHECKSUM, csum, 1)) {
        return 0;
    }
    return csum[0];
}

uint16_t BQ27441::readBlockData(uint16_t offset) {
    uint16_t address = offset + BQ27441_EXTENDED_BLOCKDATA;
    uint8_t ret[1];
    if (!readReg(address, ret, 1)) {
        return 0;
    }
    return ret[0];
}

uint16_t BQ27441::writeBlockData(uint16_t offset, uint16_t data) {
    uint16_t address = offset + BQ27441_EXTENDED_BLOCKDATA;
    uint8_t buf[1] = {(uint8_t)data};
    return writeReg(address, buf, 1);
}

uint16_t BQ27441::writeBlockChecksum(uint8_t csum) {
    return writeReg(BQ27441_EXTENDED_CHECKSUM, &csum, 1);
}

uint16_t BQ27441::computeBlockChecksum() {
    uint8_t data[32];
    if (!readReg(BQ27441_EXTENDED_BLOCKDATA, data, 32)) {
        return 0;
    }
    uint16_t csum = 0;
    for (int i = 0; i < 32; i++) {
        csum += data[i];
    }
    csum = (255 - (csum & 0xFF)) & 0xFF;
    return csum;
}

uint16_t BQ27441::readExtendedData(uint16_t class_id, uint16_t offset) {
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

    blockDataOffset(offset / 32);

    computeBlockChecksum();
    blockDataChecksum();

    uint16_t ret_data = readBlockData(offset % 32);

    if (entered_config) {
        exitConfig();
    }

    return ret_data;
}

uint16_t BQ27441::writeExtendedData(uint16_t class_id, uint16_t offset, const uint8_t* data,
                                    uint16_t length) {
    if (length > 32) {
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

    blockDataOffset(offset / 32);
    computeBlockChecksum();
    blockDataChecksum();

    for (int i = 0; i < length; i++) {
        if (!writeBlockData((offset % 32) + i, data[i])) {
            return bail(false);
        }
    }

    uint16_t new_csum = computeBlockChecksum();
    if (!writeBlockChecksum(new_csum)) {
        return bail(false);
    }

    if (entered_config) {
        exitConfig();
    }

    return true;
}

bool BQ27441::readReg(uint8_t sub_address, uint8_t* buf, uint16_t count) {
    _wire.beginTransmission(_address);
    _wire.write(sub_address);
    if (_wire.endTransmission(false) != 0)
        return false;
    // Short reads return -1 from Wire.read() which becomes 0xFF when
    // assigned to a uint8_t, so the caller would silently parse garbage.
    // Treat any partial response as a hard failure.
    if (_wire.requestFrom(_address, count) != count) {
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

bool BQ27441::writeReg(uint8_t sub_address, const uint8_t* buf, uint16_t count) {
    _wire.beginTransmission(_address);
    _wire.write(sub_address);
    for (uint16_t i = 0; i < count; i++) {
        _wire.write(buf[i]);
    }
    return _wire.endTransmission() == 0;
}

uint16_t BQ27441::status() {
    return controlStatus();
}

bool BQ27441::dataReady() {
    return (status() & BQ27441_STATUS_INITCOMP) != 0;
}