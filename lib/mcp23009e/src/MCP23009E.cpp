// SPDX-License-Identifier: GPL-3.0-or-later

#include "MCP23009E.h"

#include <math.h>

MCP23009Config::MCP23009Config(uint8_t reg) : _reg(reg) {}

MCP23009Config& MCP23009Config::setSeqop() {
    // Active SEQOP : l'adresse du pointeur ne s'incrémente pas
    _reg |= MCP23009_IOCON_SEQOP;
    return *this;
}

MCP23009Config& MCP23009Config::clearSeqop() {
    // Désactive SEQOP : l'adresse du pointeur s'incrémente
    _reg &= ~MCP23009_IOCON_SEQOP;
    return *this;
}

bool MCP23009Config::hasSeqop() const {
    // Vérifie si SEQOP est activé
    return (_reg & MCP23009_IOCON_SEQOP) != 0;
}

MCP23009Config& MCP23009Config::setOdr() {
    // Active ODR : sortie INT en drain ouvert
    _reg |= MCP23009_IOCON_ODR;
    return *this;
}

MCP23009Config& MCP23009Config::clearOdr() {
    // Désactive ODR : sortie INT en active driver
    _reg &= ~MCP23009_IOCON_ODR;
    return *this;
}

bool MCP23009Config::hasOdr() const {
    // Vérifie si ODR est activé
    return (_reg & MCP23009_IOCON_ODR) != 0;
}

MCP23009Config& MCP23009Config::setIntpol() {
    // Configure la polarité INT à Active-High
    _reg |= MCP23009_IOCON_INTPOL;
    return *this;
}

MCP23009Config& MCP23009Config::clearIntpol() {
    // Configure la polarité INT à Active-Low
    _reg &= ~MCP23009_IOCON_INTPOL;
    return *this;
}

bool MCP23009Config::hasIntpol() const {
    // Vérifie si INTPOL est configuré à Active-High
    return (_reg & MCP23009_IOCON_INTPOL) != 0;
}

MCP23009Config& MCP23009Config::setIntcc() {
    // Active INTCC : lecture de INTCAP efface l'interruption
    _reg |= MCP23009_IOCON_INTCC;
    return *this;
}

MCP23009Config& MCP23009Config::clearIntcc() {
    // Désactive INTCC : lecture de GPIO efface l'interruption
    _reg &= ~MCP23009_IOCON_INTCC;
    return *this;
}

bool MCP23009Config::hasIntcc() const {
    // Vérifie si INTCC est activé
    return (_reg & MCP23009_IOCON_INTCC) != 0;
}

uint8_t MCP23009Config::getRegisterValue() const {
    // Retourne la valeur du registre
    return _reg;
}

MCP23009E::MCP23009E(TwoWire& wire, uint8_t resetPin, uint8_t address, int interruptPin)
    : _wire(&wire), _resetPin(resetPin), _address(address), _interruptPin(interruptPin) {}

bool MCP23009E::begin() {
    pinMode(_resetPin, OUTPUT);

    if (_interruptPin != -1) {
        pinMode(_interruptPin, INPUT);
        // STM32duino's attachInterrupt() is typedef'd as
        // void(uint32_t, callback_function_t, uint32_t) where
        // callback_function_t = std::function<void(void)>, so capturing
        // lambdas are supported on the target. The native mock matches
        // that signature.
        //
        // Trigger on CHANGE so the MCU-side ISR fires regardless of
        // the user-configurable INTPOL bit (CTRL2 / MCP23009Config).
        // Whether INT is active-high or active-low, both edges land in
        // the ISR and the deferred poll() reads INTF/INTCAP to figure
        // out which pin actually fired and at what level.
        attachInterrupt(digitalPinToInterrupt(_interruptPin), [this]() { irqHandler(); }, CHANGE);
    }

    reset();

    // Probe the I2C bus: an address-only transmission ACK'd by the
    // device means it's present. Returns false otherwise so callers
    // can react to a missing/incorrectly-wired expander.
    _wire->beginTransmission(_address);
    return _wire->endTransmission() == 0;
}

uint8_t MCP23009E::setBit(uint8_t reg, uint8_t bit, uint8_t value) {
    if (value == 0) {
        reg &= ~(1 << bit);
    } else {
        reg |= (1 << bit);
    }
    return reg;
}

uint8_t MCP23009E::getBit(uint8_t reg, uint8_t bit) {
    return (reg & (1 << bit)) ? 1 : 0;
}

void MCP23009E::reset() {
    // Effectue un reset hardware du MCP23009E
    digitalWrite(_resetPin, LOW);
    delay(5);
    digitalWrite(_resetPin, HIGH);
    delay(5);
}

void MCP23009E::powerOff() {
    digitalWrite(_resetPin, LOW);
}

void MCP23009E::powerOn() {
    digitalWrite(_resetPin, HIGH);
    delay(10);
}

void MCP23009E::softReset() {
    // Réinitialise le composant avec les valeurs par défaut
    writeReg(MCP23009_IODIR, 0xFF);
    writeReg(MCP23009_GPPU, 0x00);
    writeReg(MCP23009_IOCON, 0x00);
    writeReg(MCP23009_GPINTEN, 0x00);
}

void MCP23009E::setup(uint8_t gpx, uint8_t direction, uint8_t pullup, uint8_t polarity) {
    /*Configure un GPIO

    Args:
    gpx: Numéro de GPIO (0 à 7)
    direction: MCP_DIR_INPUT ou MCP_DIR_OUTPUT
    pullup: MCP_PULLUP ou MCP_NO_PULLUP (défaut: MCP_NO_PULLUP)
    polarity: MCP_POL_SAME ou MCP_POL_INVERTED (défaut: MCP_POL_SAME)*/

    if (gpx > 7) {
        return;
    }

    uint8_t iodir = readReg(MCP23009_IODIR);
    uint8_t gppu = readReg(MCP23009_GPPU);
    uint8_t ipol = readReg(MCP23009_IPOL);

    iodir = setBit(iodir, gpx, direction);
    gppu = setBit(gppu, gpx, pullup);
    ipol = setBit(ipol, gpx, polarity);

    writeReg(MCP23009_IODIR, iodir);
    writeReg(MCP23009_GPPU, gppu);
    writeReg(MCP23009_IPOL, ipol);
}

void MCP23009E::setLevel(uint8_t gpx, uint8_t level) {
    // Drive the output level on a pin configured as output. The RMW
    // reads OLAT (the output latch) — reading GPIO would return the
    // physical pin states, which on a mixed input/output port could
    // latch input levels into OLAT and drive surprise values once an
    // input is reconfigured as an output. Per the MCP23009 datasheet
    // writing to GPIO is equivalent to writing OLAT, so the write
    // stays on GPIO for clarity at the call site.
    if (gpx > 7) {
        return;
    }

    uint8_t iodir = readReg(MCP23009_IODIR);
    if (getBit(iodir, gpx) == MCP23009_DIR_INPUT) {
        return;
    }

    uint8_t olat = readReg(MCP23009_OLAT);
    olat = setBit(olat, gpx, level);
    writeReg(MCP23009_OLAT, olat);
}

uint8_t MCP23009E::getLevel(uint8_t gpx) {
    /*Lit le niveau logique d'un GPIO

        Args:
            gpx: Numéro de GPIO (0 à 7)

        Returns:
            MCP_LOGIC_LOW ou MCP_LOGIC_HIGH*/

    if (gpx > 7) {
        return MCP23009_LOGIC_LOW;
    }

    uint8_t iodir = readReg(MCP23009_IODIR);
    if (getBit(iodir, gpx) == MCP23009_DIR_OUTPUT) {
        return getBit(readReg(MCP23009_OLAT), gpx);
    }
    return getBit(readReg(MCP23009_GPIO), gpx);
}

void MCP23009E::writeReg(uint8_t reg, uint8_t value) {
    // Écrit une valeur dans un registre
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
}

uint8_t MCP23009E::readReg(uint8_t reg) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    // Short reads return -1 from Wire.read() which becomes 0xFF when
    // assigned to uint8_t — a perfectly valid-looking register value.
    // Reject the transaction on any partial response.
    if (_wire->requestFrom(_address, static_cast<uint8_t>(1)) != 1) {
        return 0;
    }
    if (!_wire->available()) {
        return 0;
    }
    return static_cast<uint8_t>(_wire->read());
}

void MCP23009E::setIodir(uint8_t value) {
    // Définit le registre IODIR (Input/Output Direction)
    writeReg(MCP23009_IODIR, value);
}

uint8_t MCP23009E::getIodir() {
    // Lit le registre IODIR (Input/Output Direction)
    return readReg(MCP23009_IODIR);
}

void MCP23009E::setIpol(uint8_t value) {
    // Définit le registre IPOL (Input Polarity)
    writeReg(MCP23009_IPOL, value);
}

uint8_t MCP23009E::getIpol() {
    // Lit le registre IPOL (Input Polarity)
    return readReg(MCP23009_IPOL);
}

void MCP23009E::setGpinten(uint8_t value) {
    // Définit le registre GPINTEN (GPIO Interrupt Enable)
    writeReg(MCP23009_GPINTEN, value);
}

uint8_t MCP23009E::getGpinten() {
    // Lit le registre GPINTEN (GPIO Interrupt Enable)
    return readReg(MCP23009_GPINTEN);
}

void MCP23009E::setDefval(uint8_t value) {
    // Définit le registre DEFVAL (Default Value)
    writeReg(MCP23009_DEFVAL, value);
}

uint8_t MCP23009E::getDefval() {
    // Lit le registre DEFVAL (Default Value)
    return readReg(MCP23009_DEFVAL);
}

void MCP23009E::setIntcon(uint8_t value) {
    // Définit le registre INTCON (Interrupt Control)
    writeReg(MCP23009_INTCON, value);
}

uint8_t MCP23009E::getIntcon() {
    // Lit le registre INTCON (Interrupt Control)
    return readReg(MCP23009_INTCON);
}

void MCP23009E::setIocon(MCP23009Config config) {
    /*Définit le registre IOCON (I/O Configuration)

        Args:
            config: Instance de MCP23009Config ou valeur entière*/
    writeReg(MCP23009_IOCON, config.getRegisterValue());
}

MCP23009Config MCP23009E::getIocon() {
    /*Lit le registre IOCON (I/O Configuration)

            Returns:
                Instance de MCP23009Config*/

    return MCP23009Config(readReg(MCP23009_IOCON));
}

void MCP23009E::setGppu(uint8_t value) {
    // Définit le registre GPPU (GPIO Pull-Up)
    writeReg(MCP23009_GPPU, value);
}

uint8_t MCP23009E::getGppu() {
    // Lit le registre GPPU (GPIO Pull-Up)
    return readReg(MCP23009_GPPU);
}

uint8_t MCP23009E::getIntf() {
    // Lit le registre INTF (Interrupt Flag)
    return readReg(MCP23009_INTF);
}

uint8_t MCP23009E::getIntcap() {
    // Lit le registre INTCAP (Interrupt Captured value)
    return readReg(MCP23009_INTCAP);
}

void MCP23009E::setGpio(uint8_t value) {
    // Définit le registre GPIO
    writeReg(MCP23009_GPIO, value);
}

uint8_t MCP23009E::getGpio() {
    // Lit le registre GPIO
    return readReg(MCP23009_GPIO);
}

void MCP23009E::setOlat(uint8_t value) {
    // Définit le registre OLAT (Output Latch)
    writeReg(MCP23009_OLAT, value);
}

uint8_t MCP23009E::getOlat() {
    // Lit le registre OLAT (Output Latch)
    return readReg(MCP23009_OLAT);
}

void MCP23009E::sendEnableInterrupt(uint8_t gpx) {
    /* Active l'interruption pour un GPIO spécifique

        Args:
            gpx: Numéro de GPIO (0 à 7)*/

    uint8_t gpinten = readReg(MCP23009_GPINTEN);
    uint8_t intcon = readReg(MCP23009_INTCON);

    gpinten = setBit(gpinten, gpx, MCP23009_INTEN_ENABLE);
    intcon = setBit(intcon, gpx, MCP23009_INTCON_PREVIOUS_STATE);

    writeReg(MCP23009_GPINTEN, gpinten);
    writeReg(MCP23009_INTCON, intcon);
}

void MCP23009E::sendDisableInterrupt(uint8_t gpx) {
    /* Désactive l'interruption pour un GPIO spécifique

        Args:
            gpx: Numéro de GPIO (0 à 7)*/

    uint8_t gpinten = readReg(MCP23009_GPINTEN);
    uint8_t intcon = readReg(MCP23009_INTCON);
    uint8_t defval = readReg(MCP23009_DEFVAL);

    gpinten = setBit(gpinten, gpx, MCP23009_INTEN_DISABLE);
    intcon = setBit(intcon, gpx, MCP23009_INTCON_PREVIOUS_STATE);
    defval = setBit(defval, gpx, MCP23009_LOGIC_LOW);

    writeReg(MCP23009_GPINTEN, gpinten);
    writeReg(MCP23009_INTCON, intcon);
    writeReg(MCP23009_DEFVAL, defval);
}

void MCP23009E::interruptOnChange(uint8_t gpx, std::function<void(uint8_t)> callback) {
    /* Active et enregistre un callback pour les changements d'état d'un GPIO

        Args:
            gpx: Numéro de GPIO (0 à 7)
            callback: Fonction appelée lors d'un changement (reçoit le niveau logique en paramètre)
       */

    if (gpx > 7) {
        return;
    }

    sendEnableInterrupt(gpx);
    _eventsChange[gpx] = std::move(callback);
}

void MCP23009E::interruptOnFalling(uint8_t gpx, std::function<void()> callback) {
    /*Active et enregistre un callback pour les fronts descendants d'un GPIO

        Args:
            gpx: Numéro de GPIO (0 à 7)
            callback: Fonction appelée lors d'un front descendant (pas de paramètre)*/

    if (gpx > 7) {
        return;
    }
    sendEnableInterrupt(gpx);
    _eventsFall[gpx] = std::move(callback);
}

void MCP23009E::interruptOnRaising(uint8_t gpx, std::function<void()> callback) {
    /*Active et enregistre un callback pour les fronts montants d'un GPIO

        Args:
            gpx: Numéro de GPIO (0 à 7)
            callback: Fonction appelée lors d'un front montant (pas de paramètre)*/

    if (gpx > 7) {
        return;
    }

    sendEnableInterrupt(gpx);
    _eventsRise[gpx] = std::move(callback);
}

void MCP23009E::disableInterrupt(uint8_t gpx) {
    /*Désactive et désenregistre tous les callbacks pour un GPIO

        Args:
            gpx: Numéro de GPIO (0 à 7)*/

    if (gpx > 7) {
        return;
    }

    sendDisableInterrupt(gpx);

    _eventsChange[gpx] = nullptr;
    _eventsFall[gpx] = nullptr;
    _eventsRise[gpx] = nullptr;
}

void MCP23009E::irqHandler() {
    // STM32duino calls this from ISR context. Doing I2C transactions
    // or running user-provided callbacks here can deadlock (Wire uses
    // interrupts internally) or block real-time work. Just flag and
    // defer — the application drains via poll() from loop().
    _irqPending = true;
}

void MCP23009E::poll() {
    // Non-ISR dispatcher. Call this from loop() (or any non-interrupt
    // context) — it picks up the pending flag set by irqHandler() and
    // runs interruptEvent() with the bus + callback rules of normal
    // user code.
    if (_irqPending) {
        _irqPending = false;
        interruptEvent();
    }
}

void MCP23009E::interruptEvent() {
    // Reads which pins fired (INTF) and which value they had at the
    // moment of the event (INTCAP — the captured snapshot, not the
    // current GPIO state, so a bounce or follow-up edge can't
    // misclassify the callback). INTCAP is the authoritative source
    // whether INTCC is set or not.
    uint8_t intf = readReg(MCP23009_INTF);
    uint8_t state = readReg(MCP23009_INTCAP);

    for (uint8_t i = 0; i < 8; i++) {
        if (getBit(intf, i)) {
            uint8_t level = getBit(state, i);

            if (level == MCP23009_LOGIC_HIGH) {
                if (_eventsRise[i] != nullptr) {
                    _eventsRise[i]();
                }
            } else {
                if (_eventsFall[i] != nullptr) {
                    _eventsFall[i]();
                }
            }

            if (_eventsChange[i] != nullptr) {
                _eventsChange[i](level);
            }
        }
    }
}

MCP23009Pin::MCP23009Pin(MCP23009E& mcp, uint8_t pinNumber, uint8_t mode, uint8_t pull,
                         uint8_t value)
    : _mcp(mcp), _pinNumber(pinNumber), _mode(mode), _pull(pull), _value(value) {
    if (_mode != 0xFF) {
        init(_mode, _pull, _value);
    }
}

MCP23009Pin::~MCP23009Pin() {
    // If irq() was ever called on this Pin, the parent expander still
    // holds [this]-capturing lambdas in its callback arrays. Strip
    // them on destruction so a deferred poll() doesn't invoke a dead
    // object. Safe to call unconditionally: disableInterrupt is a
    // no-op when no callback was registered.
    _mcp.disableInterrupt(_pinNumber);
}

void MCP23009Pin::init(uint8_t mode, uint8_t pull, uint8_t value) {
    /*(Re)configure la pin

        Args:
            mode: Mode de la pin (IN ou OUT)
            pull: Configuration du pull-up (PULL_UP ou -1)
            value: Valeur initiale si en mode OUT*/

    if (mode != 0xFF) {
        _mode = mode;
    }
    if (pull != 0xFF) {
        _pull = pull;
    } else {
        // pull == 0xFF — sentinel for "no pull-up requested".
        _pull = MCP23009_NO_PULLUP;
    }

    uint8_t pullup = (_pull == PULL_UP) ? MCP23009_PULLUP : MCP23009_NO_PULLUP;

    _mcp.setup(_pinNumber, _mode, pullup);

    if (value != 0xFF && _mode == OUT) {
        this->value(value);
    }
}

uint8_t MCP23009Pin::value(uint8_t x) {
    /*Obtient ou définit la valeur de la pin

        Args:
            x: Nouvelle valeur (0 ou 1), ou None pour lire

        Returns:
            Si x est None, retourne la valeur actuelle (0 ou 1)
            Sinon, retourne None après avoir défini la valeur*/

    if (x == 0xff) {
        return _mcp.getLevel(_pinNumber);
    }
    uint8_t level = (x) ? MCP23009_LOGIC_HIGH : MCP23009_LOGIC_LOW;
    _mcp.setLevel(_pinNumber, level);
    return 0;
}

void MCP23009Pin::on() {
    // Met la pin à l'état haut (1)
    value(1);
}

void MCP23009Pin::off() {
    // Met la pin à l'état bas (0)
    value(0);
}

void MCP23009Pin::toggle() {
    // Inverse l'état de la pin
    value(1 - value());
}

void MCP23009Pin::irq(std::function<void()> handler, uint16_t trigger, [[maybe_unused]] bool hard) {
    /*Configure une interruption sur la pin

        Args:
            handler: Fonction appelée lors de l'interruption (reçoit la pin en paramètre)
            trigger: Type de déclenchement (IRQ_FALLING, IRQ_RISING, ou les deux)
            hard: Non utilisé (pour compatibilité avec machine.Pin)

        Returns:
            Un objet callback (cette instance)

        Exemple:
            >>> def callback(pin):
            ...     print(f"Interruption sur pin {pin._pin_number}")
            >>> pin.irq(handler=callback, trigger=MCP23009Pin.IRQ_FALLING)*/

    _irqHandler = std::move(handler);
    _irqTrigger = trigger;

    if (trigger & IRQ_FALLING) {
        if (trigger & IRQ_RISING) {
            _mcp.interruptOnChange(_pinNumber, [this](uint8_t level) {
                if (_irqHandler)
                    _irqHandler();
            });
        } else {
            _mcp.interruptOnFalling(_pinNumber, [this]() {
                if (_irqHandler)
                    _irqHandler();
            });
        }
    } else if (trigger & IRQ_RISING) {
        _mcp.interruptOnRaising(_pinNumber, [this]() {
            if (_irqHandler)
                _irqHandler();
        });
    }
}

uint8_t MCP23009Pin::mode(uint8_t mode) {
    /*Obtient ou définit le mode de la pin

        Args:
            mode: Nouveau mode (IN ou OUT), ou None pour lire

        Returns:
            Le mode actuel si mode est None*/
    if (mode == 0xff) {
        return _mode;
    }
    init(mode, _pull, 0xff);
    return _mode;
}

uint8_t MCP23009Pin::pull(uint8_t pull) {
    /*Obtient ou définit la configuration du pull-up

        Args:
            pull: Nouvelle configuration (PULL_UP ou -1), ou None pour lire

        Returns:
            La configuration actuelle si pull est None*/

    if (pull == 0xff) {
        return _pull;
    }
    init(_mode, pull, 0xff);
    return _pull;
}

MCP23009ActiveLowPin::MCP23009ActiveLowPin(MCP23009E& mcp, uint8_t pinNumber, uint8_t mode,
                                           uint8_t pull, uint8_t value)
    : _mcp(mcp),
      _pinNumber(pinNumber),
      _mode(mode),
      _pull(pull),
      _value(value),
      _pin(mcp, pinNumber, 0xFF, 0xFF, 0xFF) {
    if (_mode == 0xff) {
        _mode = MCP23009Pin::OUT;
    }
    _pin.init(_mode, _pull, 0xFF);
    if (_value != 0xff) {
        this->value(_value);
    } else if (_mode == MCP23009Pin::OUT) {
        _pin.value(1);
    }
}

void MCP23009ActiveLowPin::init(uint8_t mode, uint8_t pull, uint8_t value) {
    /*(Re)configure la pin

        Args:
            mode: Mode de la pin (IN ou OUT)
            pull: Configuration du pull-up
            value: Valeur initiale logique (inversée avant application)*/

    _pin.init(mode, pull, 0xff);

    if (value != 0xff) {
        this->value(value);
    }
}

uint8_t MCP23009ActiveLowPin::value(uint8_t x) {
    /*Obtient ou définit la valeur logique de la pin (avec inversion)

        Args:
            x: Nouvelle valeur logique (1=on, 0=off), ou None pour lire

        Returns:
            Si x est None, retourne la valeur logique actuelle
            (inversée par rapport au GPIO physique)*/

    if (x == 0xff) {
        return 1 - _pin.value();
    }
    _pin.value(1 - x);
    return 0;
}

void MCP23009ActiveLowPin::on() {
    // Active la sortie (GPIO LOW pour active-low)
    _pin.value(0);
}

void MCP23009ActiveLowPin::off() {
    // Désactive la sortie (GPIO HIGH pour active-low)
    _pin.value(1);
}

void MCP23009ActiveLowPin::toggle() {
    // Inverse l'état de la pin
    _pin.toggle();
}

void MCP23009ActiveLowPin::irq(std::function<void()> handler, uint16_t trigger, bool hard) {
    /*Configure une interruption sur la pin

        ATTENTION: Les triggers sont inversés en mode active-low !
        - IRQ_FALLING sur la pin logique = IRQ_RISING sur le GPIO physique
        - IRQ_RISING sur la pin logique = IRQ_FALLING sur le GPIO physique

        Args:
            handler: Fonction appelée lors de l'interruption
            trigger: Type de déclenchement (inversé automatiquement)
            hard: Non utilisé (pour compatibilité)

        Returns:
            Un objet callback*/

    // 0xFFFF is the header sentinel for "no override" (matches the
    // default argument); skip inversion in that case. A narrower
    // 0xff literal compared against a uint16_t would mis-match the
    // header default and always invert.
    if (trigger != 0xFFFF) {
        uint16_t invertedTrigger = 0;
        if (trigger & IRQ_FALLING) {
            invertedTrigger |= IRQ_RISING;
        }
        if (trigger & IRQ_RISING) {
            invertedTrigger |= IRQ_FALLING;
        }
        trigger = invertedTrigger;
    }
    _pin.irq(std::move(handler), trigger, hard);
}

uint8_t MCP23009ActiveLowPin::mode(uint8_t mode) {
    /*Obtient ou définit le mode de la pin

        Args:
            mode: Nouveau mode (IN ou OUT), ou None pour lire

        Returns:
            Le mode actuel si mode est None*/

    return _pin.mode(mode);
}

uint8_t MCP23009ActiveLowPin::pull(uint8_t pull) {
    /*Obtient ou définit la configuration du pull-up

        Args:
            pull: Nouvelle configuration (PULL_UP ou -1), ou None pour lire

        Returns:
            La configuration actuelle si pull est None*/

    return _pin.pull(pull);
}

uint8_t MCP23009ActiveLowPin::pinNumber() {
    // Retourne le numéro du GPIO
    return _pinNumber;
}