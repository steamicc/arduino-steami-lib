// SPDX-License-Identifier: GPL-3.0-or-later

#include "MCP23009E.h"

#include <math.h>

MCP23009Config::MCP23009Config(uint8_t reg) : _reg(reg) {}

MCP23009Config& MCP23009Config::setSeqop() {
    // Active SEQOP : l'adresse du pointeur ne s'incrémente pas
    _reg |= 0x20;
    return *this;
}

MCP23009Config& MCP23009Config::clearSeqop() {
    // Désactive SEQOP : l'adresse du pointeur s'incrémente
    _reg &= ~0x20;
    return *this;
}

bool MCP23009Config::hasSeqop() {
    // Vérifie si SEQOP est activé
    return (_reg & 0x20) > 0;
}

MCP23009Config& MCP23009Config::setOdr() {
    // Active ODR : sortie INT en drain ouvert
    _reg |= 0x04;
    return *this;
}

MCP23009Config& MCP23009Config::clearOdr() {
    // Désactive ODR : sortie INT en active driver
    _reg &= ~0x04;
    return *this;
}

bool MCP23009Config::hasOdr() {
    // Vérifie si ODR est activé
    return (_reg & 0x04) > 0;
}

MCP23009Config& MCP23009Config::setIntpol() {
    // Configure la polarité INT à Active-High
    _reg |= 0x02;
    return *this;
}

MCP23009Config& MCP23009Config::clearIntpol() {
    // Configure la polarité INT à Active-Low
    _reg &= ~0x02;
    return *this;
}

bool MCP23009Config::hasIntpol() {
    // Vérifie si INTPOL est configuré à Active-High
    return (_reg & 0x02) > 0;
}

MCP23009Config& MCP23009Config::setIntcc() {
    // Active INTCC : lecture de INTCAP efface l'interruption
    _reg |= 0x01;
    return *this;
}

MCP23009Config& MCP23009Config::clearIntcc() {
    // Désactive INTCC : lecture de GPIO efface l'interruption
    _reg &= ~0x01;
    return *this;
}

bool MCP23009Config::hasIntcc() {
    // Vérifie si INTCC est activé
    return (_reg & 0x01) > 0;
}

uint8_t MCP23009Config::getRegisterValue() {
    // Retourne la valeur du registre
    return _reg;
}

MCP23009E::MCP23009E(TwoWire& wire, uint8_t resetPin, uint8_t address, int interruptPin)
    : _wire(wire), _resetPin(resetPin), _address(address), _interruptPin(interruptPin) {}

bool MCP23009E::begin() {
    pinMode(_resetPin, OUTPUT);

    if (_interruptPin != -1) {
        pinMode(_interruptPin, INPUT);
        attachInterrupt(digitalPinToInterrupt(_interruptPin), [this]() { irqHandler(); }, FALLING);
    }

    reset();
    return true;
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
    /*Définit le niveau logique d'un GPIO configuré en sortie

        Args:
            gpx: Numéro de GPIO (0 à 7)
            level: MCP_LOGIC_LOW ou MCP_LOGIC_HIGH*/
    if (gpx > 7) {
        return;
    }

    uint8_t iodir = readReg(MCP23009_IODIR);
    if (getBit(iodir, gpx) == MCP23009_DIR_INPUT) {
        return;
    }

    uint8_t gpio = readReg(MCP23009_GPIO);
    gpio = setBit(gpio, gpx, level);
    writeReg(MCP23009_GPIO, gpio);
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

    uint8_t gpio = readReg(MCP23009_GPIO);
    return getBit(gpio, gpx);
}

void MCP23009E::writeReg(uint8_t reg, uint8_t value) {
    // Écrit une valeur dans un registre
    _wire.beginTransmission(_address);
    _wire.write(reg);
    _wire.write(value);
    _wire.endTransmission();
}

uint8_t MCP23009E::readReg(uint8_t reg) {
    // Lit une valeur depuis un registre
    _wire.beginTransmission(_address);
    _wire.write(reg);
    _wire.endTransmission(false);
    _wire.requestFrom(_address, (uint8_t)1);
    return _wire.read();
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
    /*Handler d'interruption appelé par MicroPython lors d'un événement IRQ
        Ce handler appelle interrupt_event pour traiter l'interruption*/

    interruptEvent();
}

void MCP23009E::interruptEvent() {
    /*Traite les événements d'interruption du MCP23009E
        Cette méthode lit les flags d'interruption et appelle les callbacks appropriés*/

    MCP23009Config iocon = getIocon();

    uint8_t intf = readReg(MCP23009_INTF);
    uint8_t state;

    if (iocon.hasIntcc()) {
        state = readReg(MCP23009_INTCAP);
    } else {
        state = readReg(MCP23009_GPIO);
    }
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
    : _mcp(mcp), _pinNumber(pinNumber), _mode(mode), _pull(pull), _value(value) {}

bool MCP23009Pin::begin() {
    if (_mode != -1) {
        init(_mode, _pull, _value);
    }
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
    } else if (pull == 0xFF) {
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

void MCP23009Pin::irq(std::function<void()> handler, uint16_t trigger, bool hard) {
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
      _pin(mcp, pinNumber, 0xFF, 0xFF, 0xFF) {}

bool MCP23009ActiveLowPin::begin() {
    if (_mode == 0xff) {
        _mode = MCP23009Pin::OUT;
    }
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

    if (trigger != 0xff) {
        uint8_t invertedTrigger = 0;
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