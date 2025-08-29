#include "ch.h"
#include "hal.h"

#include "linboard_config.h"
//#include "can.h"

/*
 * Application entry point.
 */
int main(void)
{

  halInit();
  palSetLine(LINE_CAN_LED);
  palSetLine(LINE_LIN_LED);
  palSetLine(LINE_STATUS_LED);
  chSysInit();

  palClearLine(LINE_CAN_LED);
  palClearLine(LINE_LIN_LED);
  palClearLine(LINE_STATUS_LED);
  //InitCan();

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
