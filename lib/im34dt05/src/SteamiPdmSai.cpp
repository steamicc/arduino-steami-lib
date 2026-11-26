// SPDX-License-Identifier: BSD-3-Clause
//
// SAI1 PDM capture of the STeaMi IMP34DT05 driver.
//
// Adapted from STMicroelectronics' X-NUCLEO-IKS02A1-Audio library,
// version 1.1.1 (https://github.com/stm32duino/X-NUCLEO-IKS02A1-Audio),
// file pdm_low_level_l476rg.cpp: SAI master receiver feeding a circular
// DMA buffer, each half handed over from the half-transfer and
// transfer-complete interrupts.
//
// Changes for the STeaMi (STeaMi contributors, same licence):
// - STM32WB55 instead of STM32L476: SAI1 block A with its PDM interface
//   (CK1 on PA3, D1 on PA10, alternate function 3) instead of SAI1
//   block B on PB3/PB5, DMA1 channel 1 through the DMAMUX;
// - SAI clock from PLLSAI1, set here, without replacing the board's
//   SystemClock_Config();
// - no HAL_SAI_MspInit() nor HAL_SAI_RxCpltCallback() overrides, which
//   would clash with other code: the DMA callbacks are set directly;
// - bytes copied by the handler instead of a fixed user buffer.
//
// Original licence (LICENSE.md of the X-NUCLEO-IKS02A1-Audio library):
//
// COPYRIGHT(c) 2021 STMicroelectronics
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// Neither the name of STMicroelectronics nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "SteamiPdmSai.h"

#include <Arduino.h>

#if defined(ARDUINO_ARCH_STM32) && defined(STM32WBxx) && defined(SAI1) && \
    defined(HAL_SAI_MODULE_ENABLED) && defined(HAL_DMA_MODULE_ENABLED)
#define STEAMI_PDM_SAI_HARDWARE 1
#endif

#ifdef STEAMI_PDM_SAI_HARDWARE

#include "lock_resource.h"

namespace {

// Two blocks: the DMA fills one while the interrupt hands the other over.
constexpr uint16_t BUFFER_BYTES = 2 * IMP34DT05_SAI_BYTES_PER_BLOCK;

SAI_HandleTypeDef saiHandle;
DMA_HandleTypeDef dmaHandle;
alignas(4) uint8_t dmaBuffer[BUFFER_BYTES];
SteamiPdmSaiHandler blockHandler = nullptr;
bool initialised = false;

// MIC_CLK = PA3 (SAI1_CK1), MIC_IN = PA10 (SAI1_D1), see the STeaMi variant.
constexpr uint16_t CLOCK_PIN = GPIO_PIN_3;
constexpr uint16_t DATA_PIN = GPIO_PIN_10;

void onHalfTransfer(DMA_HandleTypeDef* /* hdma */) {
    if (blockHandler != nullptr) {
        blockHandler(dmaBuffer, IMP34DT05_SAI_BYTES_PER_BLOCK);
    }
}

void onTransferComplete(DMA_HandleTypeDef* /* hdma */) {
    if (blockHandler != nullptr) {
        blockHandler(dmaBuffer + IMP34DT05_SAI_BYTES_PER_BLOCK, IMP34DT05_SAI_BYTES_PER_BLOCK);
    }
}

// Frequency entering the PLLs (shared by the main PLL and PLLSAI1):
// the PLL source divided by M. 0 when the PLL has no source.
uint32_t pllInputHz() {
    uint32_t sourceHz;
    switch (LL_RCC_PLL_GetMainSource()) {
        case LL_RCC_PLLSOURCE_HSI:
            sourceHz = HSI_VALUE;
            break;
        case LL_RCC_PLLSOURCE_HSE:
            sourceHz = HSE_VALUE;
            break;
        case LL_RCC_PLLSOURCE_MSI:
            sourceHz = __LL_RCC_CALC_MSI_FREQ(LL_RCC_MSI_GetRange());
            break;
        default:
            return 0;
    }
    uint32_t m = (LL_RCC_PLL_GetDivider() >> RCC_PLLCFGR_PLLM_Pos) + 1U;
    return sourceHz / m;
}

// SAI1 kernel clock = PLLSAI1 "P" = 64 MHz. On the STeaMi the PLL input
// is HSI16 / 2 = 8 MHz, so N = 16 and P = 2. The board's
// SystemClock_Config() does not use PLLSAI1 (USB and RNG run on HSI48,
// the ADC on SYSCLK).
bool configureClock() {
    uint32_t inputHz = pllInputHz();
    if (inputHz == 0 || IMP34DT05_PLLSAI1_VCO_HZ % inputHz != 0) {
        return false;
    }
    uint32_t n = IMP34DT05_PLLSAI1_VCO_HZ / inputHz;
    if (n < 6 || n > 127) {
        return false;
    }

    RCC_PeriphCLKInitTypeDef clock = {};
    clock.PeriphClockSelection = RCC_PERIPHCLK_SAI1;
    clock.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLLSAI1;
    clock.PLLSAI1.PLLN = n;
    clock.PLLSAI1.PLLP = RCC_PLLP_DIV2;
    clock.PLLSAI1.PLLQ = RCC_PLLQ_DIV2;
    clock.PLLSAI1.PLLR = RCC_PLLR_DIV2;
    clock.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_SAI1CLK;

    // The radio core (CPU2) may touch RCC too: same lock as the variant.
    hsem_lock(CFG_HW_RCC_SEMID, HSEM_LOCK_DEFAULT_RETRY);
    HAL_StatusTypeDef status = HAL_RCCEx_PeriphCLKConfig(&clock);
    hsem_unlock(CFG_HW_RCC_SEMID);

    return status == HAL_OK &&
           HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1) == IMP34DT05_SAI_KERNEL_CLOCK_HZ;
}

void configurePins() {
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};
    gpio.Pin = CLOCK_PIN | DATA_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    gpio.Alternate = GPIO_AF3_SAI1;
    HAL_GPIO_Init(GPIOA, &gpio);
}

bool configureDma() {
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    dmaHandle.Instance = DMA1_Channel1;
    dmaHandle.Init.Request = DMA_REQUEST_SAI1_A;
    dmaHandle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    dmaHandle.Init.PeriphInc = DMA_PINC_DISABLE;
    dmaHandle.Init.MemInc = DMA_MINC_ENABLE;
    dmaHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    dmaHandle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    dmaHandle.Init.Mode = DMA_CIRCULAR;
    dmaHandle.Init.Priority = DMA_PRIORITY_HIGH;

    (void)HAL_DMA_DeInit(&dmaHandle);
    if (HAL_DMA_Init(&dmaHandle) != HAL_OK) {
        return false;
    }
    __HAL_LINKDMA(&saiHandle, hdmarx, dmaHandle);

    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, IMP34DT05_DMA_IRQ_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    return true;
}

// SAI1 block A, master receiver, PDM interface with one microphone pair
// on D1/CK1. Each 16-bit frame holds two 8-bit slots, one per
// microphone of the pair, MSB = oldest PDM bit. Bit clock = 64 MHz / 25
// = 2.56 MHz (NODIV = 1), i.e. 1.28 MHz on CK1.
bool configureSai() {
    __HAL_RCC_SAI1_CLK_ENABLE();

    saiHandle.Instance = SAI1_Block_A;
    __HAL_SAI_DISABLE(&saiHandle);

    saiHandle.Init.Protocol = SAI_FREE_PROTOCOL;
    saiHandle.Init.AudioMode = SAI_MODEMASTER_RX;
    saiHandle.Init.DataSize = SAI_DATASIZE_8;
    saiHandle.Init.FirstBit = SAI_FIRSTBIT_MSB;
    saiHandle.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;
    saiHandle.Init.Synchro = SAI_ASYNCHRONOUS;
    saiHandle.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
    saiHandle.Init.NoDivider = SAI_MASTERDIVIDER_DISABLE;
    saiHandle.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
    saiHandle.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_MCKDIV;
    saiHandle.Init.Mckdiv = IMP34DT05_SAI_MCKDIV;
    saiHandle.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_DISABLE;
    saiHandle.Init.MckOutput = SAI_MCK_OUTPUT_DISABLE;
    saiHandle.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
    saiHandle.Init.MonoStereoMode = SAI_STEREOMODE;
    saiHandle.Init.CompandingMode = SAI_NOCOMPANDING;
    saiHandle.Init.TriState = SAI_OUTPUT_NOTRELEASED;
    saiHandle.Init.PdmInit.Activation = ENABLE;
    saiHandle.Init.PdmInit.MicPairsNbr = 1;
    saiHandle.Init.PdmInit.ClockEnable = SAI_PDM_CLOCK1_ENABLE;

    saiHandle.FrameInit.FrameLength = IMP34DT05_SAI_FRAME_BITS;
    saiHandle.FrameInit.ActiveFrameLength = 1;
    saiHandle.FrameInit.FSDefinition = SAI_FS_STARTFRAME;
    saiHandle.FrameInit.FSPolarity = SAI_FS_ACTIVE_HIGH;
    saiHandle.FrameInit.FSOffset = SAI_FS_FIRSTBIT;

    saiHandle.SlotInit.FirstBitOffset = 0;
    saiHandle.SlotInit.SlotSize = SAI_SLOTSIZE_DATASIZE;
    saiHandle.SlotInit.SlotNumber = IMP34DT05_SAI_SLOTS;
    saiHandle.SlotInit.SlotActive = SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1;

    return HAL_SAI_Init(&saiHandle) == HAL_OK;
}

}  // namespace

extern "C" void DMA1_Channel1_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dmaHandle);
}

bool SteamiPdmSai::supported() {
    return true;
}

bool SteamiPdmSai::begin(SteamiPdmSaiHandler handler) {
    if (initialised) {
        end();
    }
    blockHandler = handler;
    saiHandle.Instance = SAI1_Block_A;
    dmaHandle.Instance = DMA1_Channel1;
    if (!configureClock()) {
        end();
        return false;
    }
    configurePins();
    if (!configureDma() || !configureSai()) {
        end();
        return false;
    }
    initialised = true;
    return true;
}

bool SteamiPdmSai::start() {
    if (!initialised) {
        return false;
    }
    if (HAL_SAI_Receive_DMA(&saiHandle, dmaBuffer, BUFFER_BYTES) != HAL_OK) {
        return false;
    }
    // HAL_SAI_Receive_DMA() routes the DMA interrupts to the global
    // HAL_SAI_Rx*Callback() hooks; take them over so that sketches and
    // other libraries keep those hooks for themselves.
    __disable_irq();
    dmaHandle.XferHalfCpltCallback = onHalfTransfer;
    dmaHandle.XferCpltCallback = onTransferComplete;
    __enable_irq();
    return true;
}

void SteamiPdmSai::stop() {
    if (initialised) {
        (void)HAL_SAI_DMAStop(&saiHandle);
    }
}

void SteamiPdmSai::end() {
    if (saiHandle.Instance == nullptr) {
        return;  // begin() never ran
    }
    stop();
    HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    (void)HAL_SAI_DeInit(&saiHandle);
    (void)HAL_DMA_DeInit(&dmaHandle);
    HAL_GPIO_DeInit(GPIOA, CLOCK_PIN | DATA_PIN);
    __HAL_RCC_SAI1_CLK_DISABLE();

    // Stop PLLSAI1 too (its VCO draws current) unless another of its
    // outputs is still in use. configureClock() restarts it.
    hsem_lock(CFG_HW_RCC_SEMID, HSEM_LOCK_DEFAULT_RETRY);
    __HAL_RCC_PLLSAI1CLKOUT_DISABLE(RCC_PLLSAI1_SAI1CLK);
    if ((RCC->PLLSAI1CFGR &
         (RCC_PLLSAI1CFGR_PLLPEN | RCC_PLLSAI1CFGR_PLLQEN | RCC_PLLSAI1CFGR_PLLREN)) == 0U) {
        __HAL_RCC_PLLSAI1_DISABLE();
    }
    hsem_unlock(CFG_HW_RCC_SEMID);

    initialised = false;
    blockHandler = nullptr;
}

#else  // !STEAMI_PDM_SAI_HARDWARE

bool SteamiPdmSai::supported() {
    return false;
}

bool SteamiPdmSai::begin(SteamiPdmSaiHandler /* handler */) {
    return false;
}

bool SteamiPdmSai::start() {
    return false;
}

void SteamiPdmSai::stop() {}

void SteamiPdmSai::end() {}

#endif  // STEAMI_PDM_SAI_HARDWARE
