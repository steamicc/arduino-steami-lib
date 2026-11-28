// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include <functional>
#include <utility>

#include "SteamiMCP23009E_const.h"

class MCP23009Config {
   public:
    MCP23009Config(uint8_t reg = 0x00);

    MCP23009Config& setSeqop();
    MCP23009Config& clearSeqop();
    bool hasSeqop() const;
    MCP23009Config& setOdr();
    MCP23009Config& clearOdr();
    bool hasOdr() const;
    MCP23009Config& setIntpol();
    MCP23009Config& clearIntpol();
    bool hasIntpol() const;
    MCP23009Config& setIntcc();
    MCP23009Config& clearIntcc();
    bool hasIntcc() const;
    uint8_t getRegisterValue() const;

   private:
    uint8_t _reg;
};

class SteamiMCP23009E {
   public:
    SteamiMCP23009E(TwoWire& wire, uint8_t resetPin, uint8_t address = MCP23009_I2C_ADDR,
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
    // Reads the GPIO register in one transaction. Returns false on a bus
    // error and leaves value unchanged.
    bool gpio(uint8_t& value);
    void setOlat(uint8_t value);
    uint8_t getOlat();
    // Interrupt callbacks. Registering stores the callback, reads and
    // clears the pending interrupt, then enables the pin: a change made
    // before the call fires no event, and the level read then becomes the
    // last known level of the pin. A registration replaces the earlier
    // callback of the same kind. reset(), powerOff() and the soft reset
    // disable every pin and remove the watch (watchChanges()): register
    // the callbacks again afterwards.
    //
    // interruptOnChange() passes the level of the event: the one captured
    // in INTCAP, or the one read in GPIO for a change the chip did not
    // signal (see interruptEvent()).
    void interruptOnChange(uint8_t gpx, std::function<void(uint8_t)> callback);
    void interruptOnFalling(uint8_t gpx, std::function<void()> callback);
    void interruptOnRising(uint8_t gpx, std::function<void()> callback);
    // Former name of interruptOnRising() (issue #183), kept for the whole
    // 1.x series and removed in 2.0.0.
    [[deprecated("use interruptOnRising(); interruptOnRaising() goes away in 2.0.0")]]
    void interruptOnRaising(uint8_t gpx, std::function<void()> callback) {
        interruptOnRising(gpx, std::move(callback));
    }
    // Disables the pin and clears its callbacks; a pin followed by
    // watchChanges() stays enabled for it.
    void disableInterrupt(uint8_t gpx);

    // A change callback beside those of the sketch, for a library that
    // shares the expander with it (SteamiButtons follows the D-PAD with
    // it): callback(gpx, level) gets the events of the pins of mask, as
    // interruptOnChange() does, after the callbacks of the sketch. A
    // registration of the sketch (interruptOn*(), MCP23009Pin::irq()) does
    // not replace it and does not take its pending events, and
    // disableInterrupt() leaves its pins enabled: both keep working. The
    // pins of mask not enabled yet are enabled as interruptOnChange() does.
    // One watch per expander: a call replaces the earlier one, and nullptr
    // removes it (its pins stay enabled). reset(), powerOff() and the soft
    // reset remove it.
    void watchChanges(uint8_t mask, void (*callback)(uint8_t gpx, uint8_t level));
    // The pins the watch follows (one bit per pin); 0 without a watch.
    uint8_t watched() const;

    // Reads INTF, INTCAP (the port captured at the interrupt), then GPIO,
    // which clears the interrupt whatever IOCON.INTCC is. For each enabled
    // pin, the driver keeps the last known level and reports, in order:
    // - the captured level, if the pin is in INTF and that level differs
    //   from the last known one;
    // - the current GPIO level, if it still differs.
    // A short tap released before the read therefore gives the press then
    // the release. While an interrupt is pending the chip does not signal
    // changes on the other pins; the GPIO read finds them.
    //
    // Reads are queued (8 records) and delivered in FIFO order: a read made
    // from inside a callback (interruptEvent(), poll() or a registration)
    // is delivered after the events already queued. Past 8 queued reads,
    // the changes are read again from GPIO once the queue is empty. Events
    // read before a pin is registered again are dropped for that pin.
    //
    // Returns false on a bus error, with no event and no level change.
    // Limit: a press and its release that both happen while an interrupt
    // is pending cancel out and leave no trace in the chip.
    bool interruptEvent();

    // Runs interruptEvent() in normal (non-interrupt) context. Call it from
    // loop(). With an interrupt pin, it reads the chip only once the ISR
    // has flagged an interrupt — the ISR itself only flags so it can finish
    // fast and so callbacks aren't run from ISR context (where Wire and
    // arbitrary user code are unsafe on STM32duino). Without an interrupt
    // pin, it reads the chip at each call while at least one pin is enabled.
    void poll();

   private:
    // Stored as a pointer (not a reference) so the class is
    // default-assignable — matches the convention in SteamiWsenHids / SteamiWsenPads
    // and the CONTRIBUTING.md "Attributes" guidance. The public
    // constructor still takes a TwoWire& to keep the usual Arduino
    // I2C interface at the call site.
    TwoWire* _wire;
    uint8_t _address;
    uint8_t _resetPin;
    int _interruptPin;

    // Set from the MCU-side ISR, drained by poll() in normal context.
    // volatile because the writer (ISR) and reader (loop) are racing.
    volatile bool _irqPending = false;

    std::function<void(uint8_t)> _eventsChange[8];
    std::function<void()> _eventsFall[8];
    std::function<void()> _eventsRise[8];
    // The watchChanges() callback, the pins it follows, and the
    // registration counter at its registration: a read made before is not
    // delivered to it.
    void (*_watch)(uint8_t, uint8_t) = nullptr;
    uint8_t _watched = 0;
    uint32_t _watchedSince = 0;

    // Pins whose interrupt is enabled (one bit per pin), and the last known
    // level of each: the one of the last reported event, or the one read
    // when the pin was enabled.
    uint8_t _enabled = 0;
    uint8_t _levels = 0;

    // Interrupt reads whose events are still to be delivered, in order.
    struct InterruptRead {
        uint8_t fromIntcap;  // pins to report with their INTCAP level
        uint8_t captured;    // INTCAP
        uint8_t fromGpio;    // pins to report with their GPIO level
        uint8_t gpio;        // GPIO
        uint32_t generation;
    };
    static constexpr uint8_t kQueueLen = 8;
    InterruptRead _queue[kQueueLen] = {};
    // Head and tail indices, modulo 2 * kQueueLen.
    uint8_t _head = 0;
    uint8_t _tail = 0;
    bool _dispatching = false;
    // Set when a read found no room in the queue.
    bool _overflow = false;
    // Registration counter, and its value at the last registration of each
    // pin: a read made before does not call the new callback.
    uint32_t _generation = 0;
    uint32_t _registered[8] = {};

    static uint8_t setBit(uint8_t reg, uint8_t bit, uint8_t value);
    static uint8_t getBit(uint8_t reg, uint8_t bit);
    void softReset();
    void writeReg(uint8_t reg, uint8_t value);
    uint8_t readReg(uint8_t reg);
    bool readReg(uint8_t reg, uint8_t& value);
    void sendEnableInterrupt(uint8_t gpx);
    void sendDisableInterrupt(uint8_t gpx);
    void clearCallbacks(uint8_t gpx);
    bool readInterrupt(uint8_t skipMask);
    void dispatchQueued();
    void dispatch(uint8_t gpx, uint8_t level, uint32_t generation);
    void irqHandler();

    // MCP23009Pin::irq() clears the callbacks of an earlier registration.
    friend class MCP23009Pin;
};

class MCP23009Pin {
   public:
    static constexpr uint8_t IN = MCP23009_DIR_INPUT;
    static constexpr uint8_t OUT = MCP23009_DIR_OUTPUT;
    static constexpr uint8_t PULL_UP = MCP23009_PULLUP;
    static constexpr uint8_t IRQ_FALLING = 1;
    static constexpr uint8_t IRQ_RISING = 2;
    // irqLevel() before any interrupt.
    static constexpr uint8_t IRQ_LEVEL_NONE = 0xFF;

    MCP23009Pin(SteamiMCP23009E& mcp, uint8_t pinNumber, uint8_t mode = 0xFF, uint8_t pull = 0xFF,
                uint8_t value = 0xFF);

    // Unregisters any irq() callback this Pin had set on the parent
    // expander so a dispatched interrupt can't invoke a destroyed
    // object. Stack-allocated Pin lifetimes therefore no longer leak
    // dangling lambdas into SteamiMCP23009E::_eventsChange/Fall/Rise.
    ~MCP23009Pin();

    // Disable copy/move — a copied Pin would share the parent's
    // callback slot and the destructor of the moved-from object would
    // unregister the callback the moved-to object still expects.
    MCP23009Pin(const MCP23009Pin&) = delete;
    MCP23009Pin& operator=(const MCP23009Pin&) = delete;
    MCP23009Pin(MCP23009Pin&&) = delete;
    MCP23009Pin& operator=(MCP23009Pin&&) = delete;

    void init(uint8_t mode = 0xFF, uint8_t pull = 0xFF, uint8_t value = 0xFF);
    uint8_t value(uint8_t x = 0xFF);
    void on();
    void off();
    void toggle();
    // Calls handler from poll() / interruptEvent() on the chosen edges.
    // The registration replaces the earlier one (callbacks and trigger),
    // and a change made before the call does not call the handler, even if
    // it is still queued. A short tap gives two events: the press, then the
    // release.
    void irq(std::function<void()> handler = nullptr, uint16_t trigger = IRQ_FALLING | IRQ_RISING,
             bool hard = false);
    // Level of the event being delivered, to read in the irq() handler: the
    // one captured in INTCAP at the interrupt, even if the pin changed
    // before the handler ran, or the one read in GPIO for a change the chip
    // did not signal. value() reads the current level. IRQ_LEVEL_NONE
    // before any interrupt.
    uint8_t irqLevel() const;
    uint8_t mode(uint8_t mode = 0xff);
    uint8_t pull(uint8_t pull = 0xff);

   private:
    SteamiMCP23009E& _mcp;
    uint8_t _pinNumber;
    uint8_t _mode;
    uint8_t _pull;
    uint8_t _value;
    std::function<void()> _irqHandler;
    uint16_t _irqTrigger = 0;
    uint8_t _irqLevel = IRQ_LEVEL_NONE;

    void deliver(uint8_t level);
};

class MCP23009ActiveLowPin {
   public:
    static constexpr uint8_t IN = MCP23009Pin::IN;
    static constexpr uint8_t OUT = MCP23009Pin::OUT;
    static constexpr uint8_t PULL_UP = MCP23009Pin::PULL_UP;
    static constexpr uint8_t IRQ_FALLING = MCP23009Pin::IRQ_FALLING;
    static constexpr uint8_t IRQ_RISING = MCP23009Pin::IRQ_RISING;
    static constexpr uint8_t IRQ_LEVEL_NONE = MCP23009Pin::IRQ_LEVEL_NONE;

    MCP23009ActiveLowPin(SteamiMCP23009E& mcp, uint8_t pinNumber, uint8_t mode = 0xFF,
                         uint8_t pull = 0xFF, uint8_t value = 0xFF);

    void init(uint8_t mode = 0xFF, uint8_t pull = 0xFF, uint8_t value = 0xFF);
    uint8_t value(uint8_t x = 0xFF);
    void on();
    void off();
    void toggle();
    void irq(std::function<void()> handler = nullptr, uint16_t trigger = 0xFFFF, bool hard = false);
    // Logical level of the event being delivered (inverted GPIO level): 1
    // if the GPIO was low for this event. IRQ_LEVEL_NONE before any
    // interrupt.
    uint8_t irqLevel() const;
    uint8_t mode(uint8_t mode = 0xFF);
    uint8_t pull(uint8_t pull = 0xFF);
    uint8_t pinNumber();

   private:
    SteamiMCP23009E& _mcp;
    uint8_t _pinNumber;
    uint8_t _mode;
    uint8_t _pull;
    uint8_t _value;
    MCP23009Pin _pin;
};