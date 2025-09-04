#pragma once

#include <cstdint>
#include "port.h"
#include "enums.h"

msg_t InitCan(CanBitrate eBitrate, bool bEnableFilters = false);
void StopCan();
void ClearCanFilters();
void SetCanFilterId(uint8_t nFilterNum, uint32_t nId, bool bExtended);
void SetCanFilterEnabled(bool bEnabled);
uint32_t GetLastCanRxTime();