// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include <functional>

#include "MCP23009E_const.h"

class MCP23009Config {
   public:
    // come of device.py
    MCP23009Config(uint8_t reg = 0x00);

    MCP23009Config& setSeqop();
    MCP23009Config& clearSeqop();
    bool hasSeqop();
    MCP23009Config& setOdr();
    MCP23009Config& clearOdr();
    bool hasOdr();
    MCP23009Config& setIntpol();
    MCP23009Config& clearIntpol();
    bool hasIntpol();
    MCP23009Config& setIntcc();
    MCP23009Config& clearIntcc();
    bool hasIntcc();
    uint8_t getRegisterValue();

   private:
    uint8_t _reg;
};

class MCP23009E {
   public:
    MCP23009E(TwoWire& wire, uint8_t resetPin, uint8_t address = MCP23009_I2C_ADDR,
              int interruptPin = -1);

    bool begin();
    void reset();
    void powerOff();
    void powerOn();
    void setup(uint8_t gpx, uint8_t direction, uint8_t pullup = MCP23009_NO_PULLUP,
               uint8_t polarity = MCP23009_POL_SAME);
    void setLevel(uint8_t gpx, uint8_t level);
    uint8_t getLevel(uint8_t gpx);

    void setIodir(uint8_t value);
    uint8_t getIodir();
    void setIpol(uint8_t value);
    uint8_t getIpol();
    void setGpinten(uint8_t value);
    uint8_t getGpinten();
    void setDefval(uint8_t value);
    uint8_t getDefval();
    void setIntcon(uint8_t value);
    uint8_t getIntcon();
    void setIocon(MCP23009Config config);
    MCP23009Config getIocon();
    void setGppu(uint8_t value);
    uint8_t getGppu();
    uint8_t getIntf();
    uint8_t getIntcap();
    void setGpio(uint8_t value);
    uint8_t getGpio();
    void setOlat(uint8_t value);
    uint8_t getOlat();
    void interruptOnChange(uint8_t gpx, std::function<void(uint8_t)> callback);
    void interruptOnFalling(uint8_t gpx, std::function<void()> callback);
    void interruptOnRaising(uint8_t gpx, std::function<void()> callback);
    void disableInterrupt(uint8_t gpx);
    void interruptEvent();

   private:
    TwoWire& _wire;
    uint8_t _address;
    uint8_t _resetPin;
    int _interruptPin;

    std::function<void(uint8_t)> _eventsChange[8];
    std::function<void()> _eventsFall[8];
    std::function<void()> _eventsRise[8];

    static uint8_t setBit(uint8_t reg, uint8_t bit, uint8_t value);
    static uint8_t getBit(uint8_t reg, uint8_t bit);
    void softReset();
    void writeReg(uint8_t reg, uint8_t value);
    uint8_t readReg(uint8_t reg);
    void sendEnableInterrupt(uint8_t gpx);
    void sendDisableInterrupt(uint8_t gpx);
    void irqHandler();
};

class MCP23009Pin {
   public:
    static constexpr uint8_t IN = MCP23009_DIR_INPUT;
    static constexpr uint8_t OUT = MCP23009_DIR_OUTPUT;
    static constexpr uint8_t PULL_UP = MCP23009_PULLUP;
    static constexpr uint8_t IRQ_FALLING = 1;
    static constexpr uint8_t IRQ_RISING = 2;

    MCP23009Pin(MCP23009E& mcp, uint8_t pinNumber, uint8_t mode, uint8_t pull, uint8_t value);

    bool begin();
    void init(uint8_t mode = 0xFF, uint8_t pull = 0xFF, uint8_t value = 0xFF);
    uint8_t value(uint8_t x = 0xFF);
    void on();
    void off();
    void toggle();
    void irq(std::function<void()> handler = nullptr, uint16_t trigger = IRQ_FALLING | IRQ_RISING,
             bool hard = false);
    uint8_t mode(uint8_t mode = 0xff);
    uint8_t pull(uint8_t pull = 0xff);

   private:
    MCP23009E& _mcp;
    uint8_t _pinNumber;
    uint8_t _mode;
    uint8_t _pull;
    uint8_t _value;
    std::function<void()> _irqHandler;
    uint16_t _irqTrigger;
};

class MCP23009ActiveLowPin {
   public:
    static constexpr uint8_t IN = MCP23009Pin::IN;
    static constexpr uint8_t OUT = MCP23009Pin::OUT;
    static constexpr uint8_t PULL_UP = MCP23009Pin::PULL_UP;
    static constexpr uint8_t IRQ_FALLING = MCP23009Pin::IRQ_FALLING;
    static constexpr uint8_t IRQ_RISING = MCP23009Pin::IRQ_RISING;

    MCP23009ActiveLowPin(MCP23009E& mcp, uint8_t pinNumber, uint8_t mode, uint8_t pull,
                         uint8_t value);

    bool begin();
    void init(uint8_t mode = 0xFF, uint8_t pull = 0xFF, uint8_t value = 0xFF);
    uint8_t value(uint8_t x = 0xFF);
    void on();
    void off();
    void toggle();
    void irq(std::function<void()> handler = nullptr, uint16_t trigger = 0xFFFF, bool hard = false);
    uint8_t mode(uint8_t mode = 0xFF);
    uint8_t pull(uint8_t pull = 0xFF);
    uint8_t pinNumber();

   private:
    MCP23009E& _mcp;
    uint8_t _pinNumber;
    uint8_t _mode;
    uint8_t _pull;
    uint8_t _value;
    MCP23009Pin _pin;
};