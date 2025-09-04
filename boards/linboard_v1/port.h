#pragma once

#include "hal.h"
#include "enums.h"

#define SYS_TIME TIME_I2MS(chVTGetSystemTimeX())

const CANConfig &GetCanConfig(CanBitrate bitrate);

const I2CConfig i2cConfig = {
    .timingr          = STM32_TIMINGR_PRESC(15U) | STM32_TIMINGR_SCLDEL(4U) | 
                        STM32_TIMINGR_SDADEL(2U) | STM32_TIMINGR_SCLH(15U) | 
                        STM32_TIMINGR_SCLL(21U),
    .cr1              = 0,
    .cr2              = 0
};