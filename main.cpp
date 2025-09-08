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

  InitCan(CanBitrate::Bitrate_500K, false);

  InitUsb();

  InitLin();

  while (true)
  {
    palSetLine(LINE_CAN_LED);
    palSetLine(LINE_LIN_LED);
    palSetLine(LINE_STATUS_LED);
    chThdSleepMilliseconds(500);
    palClearLine(LINE_CAN_LED);
    palClearLine(LINE_LIN_LED);
    palClearLine(LINE_STATUS_LED);
    chThdSleepMilliseconds(500);
  }
}
