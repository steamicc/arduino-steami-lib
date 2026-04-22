#ifndef WSEN_HIDS_H
#define WSEN_HIDS_H

#include <Arduino.h>
#include <Wire.h>

class WsenHids {
   public:
    static constexpr uint8_t DEFAULT_ADDRESS = 0x5F;

    explicit WsenHids(TwoWire& wire = Wire, uint8_t address = DEFAULT_ADDRESS);

    bool begin();
    uint8_t deviceId();

    void powerOn();
    void powerOff();
    void reboot();

    uint8_t status();
    bool temperatureReady();
    bool humidityReady();
    bool dataReady();

    float temperature();
    float humidity();

    void setContinuous(uint8_t odr);
    void triggerOneShot();
    std::tuple<float, float> readOneShot();
    void setAveraging(uint8_t humidityAvg, uint8_t temperatureAvg);

   private:
    bool readReg(uint8_t reg, uint8_t* data, size_t len = 1);
    bool writeReg(uint8_t reg, uint8_t data);
    bool writeReg(uint8_t reg, const uint8_t* data, size_t len);
    bool updateReg(uint8_t reg, uint8_t mask, uint8_t value);

    bool readU16(uint8_t lowReg, uint16_t& value);
    bool readS16(uint8_t lowReg, int16_t& value);
    bool readCalibration();

    bool _calibrationLoaded;
    TwoWire& _wire;
    uint8_t _address;

    float _h0_rh;
    float _h1_rh;
    int16_t _h0_t0_out;
    int16_t _h1_t0_out;

    float _t0_degC;
    float _t1_degC;
    int16_t _t0_out;
    int16_t _t1_out;
};

#endif
