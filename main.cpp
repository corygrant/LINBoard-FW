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
  chSysInit();

  //InitCan();

  while (true)
  {
    
    chThdSleepMilliseconds(500);
  }
}
