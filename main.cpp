#include "ch.hpp"
#include "hal.h"

#include "linboard_config.h"
#include "can.h"
#include "usb.h"
#include "lin.h"
#include "enums.h"
#include "mailbox.h"

/*
 * Application entry point.
 */
int main(void)
{

  halInit();
  chSysInit();

  InitMailboxes();

  palClearLine(LINE_CAN_STANDBY); //Enable CAN transceiver

  InitCan(CanBitrate::Bitrate_500K, false);

  InitUsb();

  InitLin();

  while (true)
  {
    if(CanRxIsActive())
        palSetLine(LINE_CAN_LED);
    else
        palClearLine(LINE_CAN_LED);

    if(LinRxIsActive())
        palSetLine(LINE_LIN_LED);
    else
        palClearLine(LINE_LIN_LED);

    CANTxFrame stMsg;
    stMsg.SID = 50;
    stMsg.DLC = 8;
    stMsg.data8[0] = 1;
    stMsg.data8[1] = 2;
    stMsg.data8[2] = 3;
    stMsg.data8[3] = 4;
    stMsg.data8[4] = 5;
    stMsg.data8[5] = 6;
    stMsg.data8[6] = 7;
    stMsg.data8[7] = 8;
    stMsg.IDE = CAN_IDE_STD;
    stMsg.RTR = CAN_RTR_DATA;
    PostTxFrame(&stMsg);

    chThdSleepMilliseconds(50);
  }
}
