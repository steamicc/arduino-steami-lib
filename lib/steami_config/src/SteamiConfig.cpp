// SPDX-License-Identifier: GPL-3.0-or-later
#include "SteamiConfig.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr const char* kSensorNames[STEAMI_CONFIG_SENSOR_COUNT] = {
    STEAMI_CONFIG_SENSOR_HTS221,    STEAMI_CONFIG_SENSOR_LIS2MDL,   STEAMI_CONFIG_SENSOR_ISM330DL,
    STEAMI_CONFIG_SENSOR_WSEN_HIDS, STEAMI_CONFIG_SENSOR_WSEN_PADS,
};

constexpr const char* kSensorShortKeys[STEAMI_CONFIG_SENSOR_COUNT] = {
    "hts", "mag", "ism", "hid", "pad",
};

void appendFloat(String& out, float value) {
    String formatted(value, 6);

    while (formatted.endsWith("0")) {
        formatted.remove(formatted.length() - 1);
    }

    if (formatted.endsWith(".")) {
        formatted.remove(formatted.length() - 1);
    }

    out += formatted;
}

}  // namespace

SteamiConfig::SteamiConfig(DaplinkBridge& bridge) : _bridge(&bridge) {}

// ---------------------------------------------------------------------
// Lifecycle / persistence
// ---------------------------------------------------------------------

bool SteamiConfig::begin() {
    if (_bridge == nullptr) {
        return false;
    }

    return _bridge->begin();
}

bool SteamiConfig::load() {
    clear();

    if (_bridge == nullptr) {
        return false;
    }

    static uint8_t raw[STEAMI_CONFIG_MAX_SIZE + 1];

    const size_t length = _bridge->readConfig(raw, STEAMI_CONFIG_MAX_SIZE);

    if (length == 0) {
        return true;
    }

    raw[length] = '\0';

    if (!parseJson(reinterpret_cast<const char*>(raw))) {
        clear();
        return false;
    }

    return true;
}
bool SteamiConfig::save() {
    if (_bridge == nullptr) {
        return false;
    }

    String json;
    if (!serializeJson(json) || json.length() >= STEAMI_CONFIG_MAX_SIZE) {
        return false;
    }

    if (!_bridge->clearConfig()) {
        return false;
    }

    return _bridge->writeConfig(reinterpret_cast<const uint8_t*>(json.c_str()), json.length());
}

void SteamiConfig::clear() {
    _hasBoardRevision = false;
    _boardRevision = 0;

    _hasBoardName = false;
    _boardName = "";

    for (uint8_t i = 0; i < STEAMI_CONFIG_SENSOR_COUNT; ++i) {
        _temperature[i] = TemperatureEntry{};
    }

    _hasMagnetometerCalibration = false;
    _magnetometerCalibration = MagnetometerCalibration{};

    _hasAccelerometerCalibration = false;
    _accelerometerCalibration = AccelerometerCalibration{};

    _hasBootCount = false;
    _bootCount = 0;
}

// ---------------------------------------------------------------------
// Board info
// ---------------------------------------------------------------------

void SteamiConfig::setBoardRevision(int32_t revision) {
    _boardRevision = revision;
    _hasBoardRevision = true;
}

bool SteamiConfig::boardRevision(int32_t& revision) const {
    if (!_hasBoardRevision) {
        return false;
    }
    revision = _boardRevision;
    return true;
}

void SteamiConfig::clearBoardRevision() {
    _hasBoardRevision = false;
    _boardRevision = 0;
}

void SteamiConfig::setBoardName(const String& name) {
    _boardName = name;
    _hasBoardName = true;
}

bool SteamiConfig::boardName(String& name) const {
    if (!_hasBoardName) {
        return false;
    }
    name = _boardName;
    return true;
}

void SteamiConfig::clearBoardName() {
    _hasBoardName = false;
    _boardName = "";
}

// ---------------------------------------------------------------------
// Temperature calibration
// ---------------------------------------------------------------------

bool SteamiConfig::setTemperatureCalibration(const char* sensor, float gain, float offset) {
    const int8_t index = sensorIndex(sensor);
    if (index < 0) {
        return false;
    }

    _temperature[index].present = true;
    _temperature[index].calibration.gain = gain;
    _temperature[index].calibration.offset = offset;
    return true;
}

bool SteamiConfig::getTemperatureCalibration(const char* sensor,
                                             TemperatureCalibration& calibration) const {
    const int8_t index = sensorIndex(sensor);
    if (index < 0 || !_temperature[index].present) {
        return false;
    }

    calibration = _temperature[index].calibration;
    return true;
}

bool SteamiConfig::clearTemperatureCalibration(const char* sensor) {
    const int8_t index = sensorIndex(sensor);
    if (index < 0) {
        return false;
    }

    _temperature[index] = TemperatureEntry{};
    return true;
}

// ---------------------------------------------------------------------
// Magnetometer calibration
// ---------------------------------------------------------------------

void SteamiConfig::setMagnetometerCalibration(float hardIronX, float hardIronY, float hardIronZ,
                                              float softIronX, float softIronY, float softIronZ) {
    _magnetometerCalibration.hardIronX = hardIronX;
    _magnetometerCalibration.hardIronY = hardIronY;
    _magnetometerCalibration.hardIronZ = hardIronZ;
    _magnetometerCalibration.softIronX = softIronX;
    _magnetometerCalibration.softIronY = softIronY;
    _magnetometerCalibration.softIronZ = softIronZ;
    _hasMagnetometerCalibration = true;
}

bool SteamiConfig::getMagnetometerCalibration(MagnetometerCalibration& calibration) const {
    if (!_hasMagnetometerCalibration) {
        return false;
    }

    calibration = _magnetometerCalibration;
    return true;
}

void SteamiConfig::clearMagnetometerCalibration() {
    _hasMagnetometerCalibration = false;
    _magnetometerCalibration = MagnetometerCalibration{};
}

// ---------------------------------------------------------------------
// Accelerometer calibration
// ---------------------------------------------------------------------

void SteamiConfig::setAccelerometerCalibration(float offsetX, float offsetY, float offsetZ) {
    _accelerometerCalibration.offsetX = offsetX;
    _accelerometerCalibration.offsetY = offsetY;
    _accelerometerCalibration.offsetZ = offsetZ;
    _hasAccelerometerCalibration = true;
}

bool SteamiConfig::getAccelerometerCalibration(AccelerometerCalibration& calibration) const {
    if (!_hasAccelerometerCalibration) {
        return false;
    }

    calibration = _accelerometerCalibration;
    return true;
}

void SteamiConfig::clearAccelerometerCalibration() {
    _hasAccelerometerCalibration = false;
    _accelerometerCalibration = AccelerometerCalibration{};
}

// ---------------------------------------------------------------------
// Boot counter
// ---------------------------------------------------------------------

void SteamiConfig::setBootCount(uint32_t count) {
    _bootCount = count;
    _hasBootCount = true;
}

bool SteamiConfig::bootCount(uint32_t& count) const {
    if (!_hasBootCount) {
        return false;
    }

    count = _bootCount;
    return true;
}

uint32_t SteamiConfig::incrementBootCount() {
    if (!_hasBootCount) {
        _bootCount = 0;
        _hasBootCount = true;
    }

    ++_bootCount;
    return _bootCount;
}

// ---------------------------------------------------------------------
// Sensor mapping
// ---------------------------------------------------------------------

int8_t SteamiConfig::sensorIndex(const char* sensor) const {
    if (sensor == nullptr) {
        return -1;
    }

    for (uint8_t i = 0; i < STEAMI_CONFIG_SENSOR_COUNT; ++i) {
        if (strcmp(sensor, kSensorNames[i]) == 0) {
            return static_cast<int8_t>(i);
        }
    }

    return -1;
}

const char* SteamiConfig::sensorShortKey(uint8_t index) const {
    if (index >= STEAMI_CONFIG_SENSOR_COUNT) {
        return nullptr;
    }
    return kSensorShortKeys[index];
}

// ---------------------------------------------------------------------
// JSON serialization
// ---------------------------------------------------------------------

bool SteamiConfig::serializeJson(String& json) const {
    json = "{";
    bool first = true;

    auto field = [&json, &first](const char* key) {
        if (!first) {
            json += ",";
        }
        first = false;
        json += "\"";
        json += key;
        json += "\":";
    };

    if (_hasBoardRevision) {
        field("rev");
        json += String(_boardRevision);
    }

    if (_hasBoardName) {
        field("name");
        appendEscapedString(json, _boardName);
    }

    bool hasTemperature = false;
    for (uint8_t i = 0; i < STEAMI_CONFIG_SENSOR_COUNT; ++i) {
        hasTemperature = hasTemperature || _temperature[i].present;
    }

    if (hasTemperature) {
        field("tc");
        json += "{";
        bool firstSensor = true;

        for (uint8_t i = 0; i < STEAMI_CONFIG_SENSOR_COUNT; ++i) {
            if (!_temperature[i].present) {
                continue;
            }

            if (!firstSensor) {
                json += ",";
            }
            firstSensor = false;

            json += "\"";
            json += sensorShortKey(i);
            json += "\":{\"g\":";
            appendFloat(json, _temperature[i].calibration.gain);
            json += ",\"o\":";
            appendFloat(json, _temperature[i].calibration.offset);
            json += "}";
        }

        json += "}";
    }

    if (_hasMagnetometerCalibration) {
        field("cm");
        json += "{\"hx\":";
        appendFloat(json, _magnetometerCalibration.hardIronX);
        json += ",\"hy\":";
        appendFloat(json, _magnetometerCalibration.hardIronY);
        json += ",\"hz\":";
        appendFloat(json, _magnetometerCalibration.hardIronZ);
        json += ",\"sx\":";
        appendFloat(json, _magnetometerCalibration.softIronX);
        json += ",\"sy\":";
        appendFloat(json, _magnetometerCalibration.softIronY);
        json += ",\"sz\":";
        appendFloat(json, _magnetometerCalibration.softIronZ);
        json += "}";
    }

    if (_hasAccelerometerCalibration) {
        field("ca");
        json += "{\"ox\":";
        appendFloat(json, _accelerometerCalibration.offsetX);
        json += ",\"oy\":";
        appendFloat(json, _accelerometerCalibration.offsetY);
        json += ",\"oz\":";
        appendFloat(json, _accelerometerCalibration.offsetZ);
        json += "}";
    }

    if (_hasBootCount) {
        field("bc");
        json += String(_bootCount);
    }

    json += "}";
    return json.length() < STEAMI_CONFIG_MAX_SIZE;
}

void SteamiConfig::appendEscapedString(String& json, const String& value) {
    json += "\"";

    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];

        switch (c) {
            case '"':
                json += "\\\"";
                break;
            case '\\':
                json += "\\\\";
                break;
            case '\b':
                json += "\\b";
                break;
            case '\f':
                json += "\\f";
                break;
            case '\n':
                json += "\\n";
                break;
            case '\r':
                json += "\\r";
                break;
            case '\t':
                json += "\\t";
                break;
            default:
                if (static_cast<uint8_t>(c) >= 0x20) {
                    json += c;
                }
                break;
        }
    }

    json += "\"";
}

// ---------------------------------------------------------------------
// JSON parsing
// ---------------------------------------------------------------------

const char* SteamiConfig::skipWhitespace(const char* cursor) {
    if (cursor == nullptr) {
        return nullptr;
    }

    while (*cursor != '\0' && isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    return cursor;
}

const char* SteamiConfig::findValue(const char* json, const char* key) {
    if (json == nullptr || key == nullptr) {
        return nullptr;
    }

    String pattern = "\"";
    pattern += key;
    pattern += "\"";

    const char* cursor = json;
    while ((cursor = strstr(cursor, pattern.c_str())) != nullptr) {
        const char* afterKey = skipWhitespace(cursor + pattern.length());
        if (afterKey != nullptr && *afterKey == ':') {
            return skipWhitespace(afterKey + 1);
        }
        cursor += pattern.length();
    }

    return nullptr;
}

bool SteamiConfig::extractObject(const char* json, const char* key, String& object) {
    const char* start = findValue(json, key);
    if (start == nullptr || *start != '{') {
        return false;
    }

    const char* cursor = start;
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    while (*cursor != '\0') {
        const char c = *cursor;

        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
        } else if (c == '"') {
            inString = true;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) {
                object = String(start).substring(0, static_cast<unsigned int>(cursor - start + 1));
                return true;
            }
        }

        ++cursor;
    }

    return false;
}

bool SteamiConfig::parseFloatValue(const char* json, const char* key, float& value) {
    const char* start = findValue(json, key);
    if (start == nullptr) {
        return false;
    }

    char* end = nullptr;
    const float parsed = strtof(start, &end);
    if (end == start) {
        return false;
    }

    value = parsed;
    return true;
}

bool SteamiConfig::parseIntValue(const char* json, const char* key, int32_t& value) {
    const char* start = findValue(json, key);
    if (start == nullptr) {
        return false;
    }

    char* end = nullptr;
    const long parsed = strtol(start, &end, 10);
    if (end == start) {
        return false;
    }

    value = static_cast<int32_t>(parsed);
    return true;
}

bool SteamiConfig::parseUintValue(const char* json, const char* key, uint32_t& value) {
    const char* start = findValue(json, key);
    if (start == nullptr || *start == '-') {
        return false;
    }

    char* end = nullptr;
    const unsigned long parsed = strtoul(start, &end, 10);
    if (end == start) {
        return false;
    }

    value = static_cast<uint32_t>(parsed);
    return true;
}

bool SteamiConfig::parseStringValue(const char* json, const char* key, String& value) {
    const char* cursor = findValue(json, key);
    if (cursor == nullptr || *cursor != '"') {
        return false;
    }

    ++cursor;
    value = "";

    while (*cursor != '\0') {
        char c = *cursor++;

        if (c == '"') {
            return true;
        }

        if (c != '\\') {
            value += c;
            continue;
        }

        const char escaped = *cursor++;
        if (escaped == '\0') {
            return false;
        }

        switch (escaped) {
            case '"':
                value += '"';
                break;
            case '\\':
                value += '\\';
                break;
            case '/':
                value += '/';
                break;
            case 'b':
                value += '\b';
                break;
            case 'f':
                value += '\f';
                break;
            case 'n':
                value += '\n';
                break;
            case 'r':
                value += '\r';
                break;
            case 't':
                value += '\t';
                break;
            default:
                return false;
        }
    }

    return false;
}

bool SteamiConfig::parseJson(const char* json) {
    if (json == nullptr) {
        return false;
    }

    const char* start = skipWhitespace(json);
    if (start == nullptr || *start != '{') {
        return false;
    }

    const char* end = json + strlen(json);
    while (end > start && isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    if (end <= start || *(end - 1) != '}') {
        return false;
    }

    int32_t revision = 0;
    if (parseIntValue(json, "rev", revision)) {
        setBoardRevision(revision);
    }

    String name;
    if (parseStringValue(json, "name", name)) {
        setBoardName(name);
    }

    String temperatureObject;
    if (extractObject(json, "tc", temperatureObject)) {
        for (uint8_t i = 0; i < STEAMI_CONFIG_SENSOR_COUNT; ++i) {
            String sensorObject;
            if (!extractObject(temperatureObject.c_str(), sensorShortKey(i), sensorObject)) {
                continue;
            }

            float gain = 0.0f;
            float offset = 0.0f;
            if (!parseFloatValue(sensorObject.c_str(), "g", gain) ||
                !parseFloatValue(sensorObject.c_str(), "o", offset)) {
                return false;
            }

            setTemperatureCalibration(kSensorNames[i], gain, offset);
        }
    }

    String magnetometerObject;
    if (extractObject(json, "cm", magnetometerObject)) {
        MagnetometerCalibration calibration;
        parseFloatValue(magnetometerObject.c_str(), "hx", calibration.hardIronX);
        parseFloatValue(magnetometerObject.c_str(), "hy", calibration.hardIronY);
        parseFloatValue(magnetometerObject.c_str(), "hz", calibration.hardIronZ);
        parseFloatValue(magnetometerObject.c_str(), "sx", calibration.softIronX);
        parseFloatValue(magnetometerObject.c_str(), "sy", calibration.softIronY);
        parseFloatValue(magnetometerObject.c_str(), "sz", calibration.softIronZ);

        setMagnetometerCalibration(calibration.hardIronX, calibration.hardIronY,
                                   calibration.hardIronZ, calibration.softIronX,
                                   calibration.softIronY, calibration.softIronZ);
    }

    String accelerometerObject;
    if (extractObject(json, "ca", accelerometerObject)) {
        AccelerometerCalibration calibration;
        parseFloatValue(accelerometerObject.c_str(), "ox", calibration.offsetX);
        parseFloatValue(accelerometerObject.c_str(), "oy", calibration.offsetY);
        parseFloatValue(accelerometerObject.c_str(), "oz", calibration.offsetZ);

        setAccelerometerCalibration(calibration.offsetX, calibration.offsetY, calibration.offsetZ);
    }

    uint32_t count = 0;
    if (parseUintValue(json, "bc", count)) {
        setBootCount(count);
    }

    return true;
}
