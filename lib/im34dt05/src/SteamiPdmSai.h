// SPDX-License-Identifier: BSD-3-Clause
//
// SAI1 PDM capture of the STeaMi IMP34DT05 driver, adapted from
// STMicroelectronics' X-NUCLEO-IKS02A1-Audio library: see
// SteamiPdmSai.cpp for the origin, the changes and the licence.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "SteamiIMP34DT05_const.h"

// Called from the DMA interrupt every IMP34DT05_BLOCK_MS with one block
// of raw SAI data: IMP34DT05_SAI_BYTES_PER_BLOCK bytes, slot 0 and slot 1
// of each SAI frame alternating.
typedef void (*SteamiPdmSaiHandler)(const uint8_t* data, size_t length);

// Low-level acquisition used by SteamiIMP34DT05. MIC_CLK (PA3) is
// SAI1_CK1 and MIC_IN (PA10) is SAI1_D1, alternate function 3; SAI1
// block A runs in PDM mode and a circular DMA (DMA1 channel 1) fills a
// two-block buffer.
//
// Only built on the STM32WB55 (STeaMi); elsewhere every function fails.
class SteamiPdmSai {
   public:
    // True when this build can drive the hardware.
    static bool supported();

    // Configure clocks, pins, DMA and SAI1. Does not start the clock.
    static bool begin(SteamiPdmSaiHandler handler);

    // Start / stop the PDM clock and the DMA. The microphone powers
    // down when its clock stops.
    static bool start();
    static void stop();

    // Stop and release SAI1, the DMA channel and the pins.
    static void end();

    // Body of the DMA1 channel 1 interrupt. The library defines
    // DMA1_Channel1_IRQHandler() to call it, unless the build defines
    // STEAMI_IMP34DT05_NO_DMA_IRQ_HANDLER (for a sketch or library that
    // has its own handler: call this function from it to keep the
    // microphone working).
    static void handleDmaInterrupt();

#if defined(UNIT_TEST) && !defined(ARDUINO_ARCH_STM32)
    // Host tests only (not in the Arduino build). hostSetPresent(true)
    // simulates a board: supported(), begin() and start() succeed and
    // begin() keeps the handler; false (the default) restores the stubs
    // that fail. hostFeed() hands one block to the handler, as the DMA
    // interrupt would, between start() and stop().
    static void hostSetPresent(bool present);
    static void hostFeed(const uint8_t* data, size_t length);
#endif
};
