// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <DaplinkBridge.h>

// Keeps a copy of the DAPLink config zone so that a test suite writing to
// it can put the board's own configuration (calibrations, name...) back
// once it is done.
struct ConfigZoneBackup {
    uint8_t data[DAPLINK_BRIDGE_CONFIG_SIZE] = {0};
    size_t length = 0;

    void take(DaplinkBridge& bridge) { length = bridge.readConfig(data, sizeof(data)); }

    bool restore(DaplinkBridge& bridge) const {
        if (!bridge.clearConfig()) {
            return false;
        }
        return length == 0 || bridge.writeConfig(data, length, 0);
    }
};
