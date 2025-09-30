#include "can.h"
#include "hal.h"
#include "port.h"
#include "mailbox.h"
#include "linboard_config.h"

#include <iterator>

#define RX_TIMEOUT_MS 100

static CANFilter canfilters[STM32_CAN_MAX_FILTERS];
static uint32_t nFilterIds[STM32_CAN_MAX_FILTERS * 2];
static bool bFilterExtended[STM32_CAN_MAX_FILTERS * 2];

static uint32_t nLastCanRxTime;
static bool bCanFilterEnabled = true;

void ConfigureCanFilters();

static THD_WORKING_AREA(waCanTxThread, 256);
void CanTxThread(void *)
{
    chRegSetThreadName("CAN Tx");

    CANTxFrame msg;

    while (1)
    {
        // Send all messages in the TX queue
        msg_t res;
        do
        {
            res = FetchTxFrame(&msg);
            if (res == MSG_OK)
            {
                msg.IDE = CAN_IDE_STD;
                msg.RTR = CAN_RTR_DATA;
                res = canTransmitTimeout(&CAND1, CAN_ANY_MAILBOX, &msg, TIME_IMMEDIATE);
                // Returns true if mailbox full or nothing connected
                // TODO: What to do if no tx?

                chThdSleepMicroseconds(CAN_TX_MSG_SPLIT);
            }
        } while (res == MSG_OK);

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);

        chThdSleepMicroseconds(30);
    }
}

static THD_WORKING_AREA(waCanRxThread, 128);
void CanRxThread(void *)
{
    CANRxFrame msg;

    chRegSetThreadName("CAN Rx");

    while (true)
    {

        msg_t res = canReceiveTimeout(&CAND1, CAN_ANY_MAILBOX, &msg, TIME_IMMEDIATE);
        if (res == MSG_OK)
        {
            nLastCanRxTime = SYS_TIME;

            res = PostRxFrame(&msg);
            //palToggleLine(LINE_E2);
            // TODO:What to do if mailbox is full?
        }

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);

        chThdSleepMicroseconds(30);
    }
}

static thread_t *canCyclicTxThreadRef;
static thread_t *canTxThreadRef;
static thread_t *canRxThreadRef;

msg_t InitCan(CanBitrate eBitrate, bool bEnableFilters)
{
    if (canCyclicTxThreadRef || canTxThreadRef || canRxThreadRef)
    {
        StopCan();
    }

    SetCanFilterEnabled(bEnableFilters);

    ConfigureCanFilters();

    msg_t ret = canStart(&CAND1, &GetCanConfig(eBitrate));
    if (ret != HAL_RET_SUCCESS)
        return ret;
    canTxThreadRef = chThdCreateStatic(waCanTxThread, sizeof(waCanTxThread), NORMALPRIO + 1, CanTxThread, nullptr);
    canRxThreadRef = chThdCreateStatic(waCanRxThread, sizeof(waCanRxThread), NORMALPRIO + 1, CanRxThread, nullptr);

    return HAL_RET_SUCCESS;
}

void StopCan()
{
    // Signal threads to terminate
    chThdTerminate(canCyclicTxThreadRef);
    chThdTerminate(canTxThreadRef);
    chThdTerminate(canRxThreadRef);

    // Wait for threads to exit
    chThdWait(canCyclicTxThreadRef);
    chThdWait(canTxThreadRef);
    chThdWait(canRxThreadRef);

    // Stop CAN driver
    canStop(&CAND1);

    // Reset thread references
    canCyclicTxThreadRef = NULL;
    canTxThreadRef = NULL;
    canRxThreadRef = NULL;
}

void ClearCanFilters()
{
    // Clear all filters
    for (uint8_t i = 0; i < STM32_CAN_MAX_FILTERS; i++)
    {
        nFilterIds[i] = 0;
        bFilterExtended[i] = false;

        canfilters[i].register1 = 0;
        canfilters[i].register2 = 0;
        canfilters[i].filter = 0;
        canfilters[i].assignment = 0;
        canfilters[i].mode = 0;
        canfilters[i].scale = 0;
    }
}

void SetCanFilterId(uint8_t nFilterNum, uint32_t nId, bool bExtended)
{
    if (nFilterNum >= (STM32_CAN_MAX_FILTERS * 2))
        return;

    bFilterExtended[nFilterNum] = bExtended;

    if (bExtended)
    {
        nFilterIds[nFilterNum] = (nId << 3) | 0x04; // Set IDE bit for extended ID
    }
    else
    {
        nFilterIds[nFilterNum] = nId << 21;
    }
}

void ConfigureCanFilters()
{

    if(!bCanFilterEnabled)
    {
        // Default HAL config = filter 0 enabled to allow all messages
        return;
    }

    uint8_t nCurrentFilter = 0;

    // Go through nFilterIds and set filter register1 and register2 for each filter if ID is set
    // CANNOT SET ALL FILTERS, MUST USE ONLY THE NUMBER OF REQUIRED FILTERS
    for (uint8_t i = 0; i < (STM32_CAN_MAX_FILTERS * 2); i += 2)
    {
        if (nFilterIds[i] != 0 || nFilterIds[i + 1] != 0)
        {
            canfilters[nCurrentFilter].filter = nCurrentFilter; // Filter bank number
            canfilters[nCurrentFilter].assignment = 0;          // Assign to FIFO 0
            canfilters[nCurrentFilter].mode = 1;                // List mode
            canfilters[nCurrentFilter].scale = 1;               // 32-bit scale

            // First ID (register1)
            if (nFilterIds[i] != 0)
            {
                canfilters[nCurrentFilter].register1 = nFilterIds[i];
            }

            // Second ID (register2)
            if (nFilterIds[i + 1] != 0)
            {
                canfilters[nCurrentFilter].register2 = nFilterIds[i + 1];
            }

            nCurrentFilter++;
        }
    }

    // Apply all filter configurations
    // If no CAN inputs are enabled, filter[0].register1 is still set to settings request message ID (BaseId-1)
    canSTM32SetFilters(&CAND1, STM32_CAN_MAX_FILTERS, nCurrentFilter, canfilters);
}

uint32_t GetLastCanRxTime()
{
    return nLastCanRxTime;
}

bool CanRxIsActive()
{
    return (SYS_TIME - nLastCanRxTime) < RX_TIMEOUT_MS;
}

void SetCanFilterEnabled(bool bEnabled)
{
    bCanFilterEnabled = bEnabled;

    // TODO: Reconfigure filters if enabled/disabled
}