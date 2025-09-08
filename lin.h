#pragma once

#include <cstdint>
#include "port.h"

#define LIN_BREAK_DURATION_US 700
#define LIN_SYNC_BYTE 0x55
#define LIN_INTERBYTE_SPACE_MS  1

typedef struct {
    uint8_t nId;
    uint8_t nData[10];
    uint8_t nLength;
    uint8_t nChecksum;
} stLinFrame;


void InitLin();