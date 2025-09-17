#include "lin.h"
#include "port.h"
#include <cstring>

uint8_t nCounter = 0;
bool bOn = false;  // Define bOn here

bool bSlowSpeed = true;
bool bFastSpeed = false;
bool bIntMode = false;
bool bSwipeMode = false;
uint8_t nIntModeSpeed = 0;

static UARTConfig lin_config = {
    .txend1_cb = NULL,      // Called when transmission completes
    .txend2_cb = NULL,
    .rxend_cb = NULL,
    .rxchar_cb = NULL,
    .rxerr_cb = NULL,
    .timeout_cb = NULL,
    .timeout = 1000,
    .speed = 19200,
    .cr1 = 0,
    .cr2 = USART_CR2_LINEN,
    .cr3 = 0
};

static void ConfigureLinMode(UARTDriver *udriver) {
    USART_TypeDef *uart = udriver->usart;



    // Disable USART first
    uartStop(&UARTD2);

    // Clear all relevant control bits first
    uart->CR1 &= ~(USART_CR1_M0 | USART_CR1_PCE | USART_CR1_PS);
    uart->CR2 &= ~(USART_CR2_STOP_0 | USART_CR2_STOP_1 | USART_CR2_CLKEN);
    uart->CR3 &= ~(USART_CR3_SCEN | USART_CR3_HDSEL | USART_CR3_IREN);

    // Configure CR2 register for LIN mode
    uart->CR2 |= USART_CR2_LINEN;    // Enable LIN mode

    // Re-enable USART
    uartStart(&UARTD2, &lin_config);

    // Small delay for mode to settle
    chThdSleepMicroseconds(100);

}

static void LinSendBreak(void)
{
    UARTD2.usart->RQR |= USART_RQR_SBKRQ;
    
    // Wait for break completion
    while(UARTD2.usart->ISR & USART_ISR_SBKF) {
        chThdSleepMicroseconds(10);
    }

    //chThdSleepMicroseconds(500);
}

// Calculate protected ID with parity bits
static uint8_t LinCalculateProtectedId(uint8_t id) {
    uint8_t p0 = (id >> 0) ^ (id >> 1) ^ (id >> 2) ^ (id >> 4);
    uint8_t p1 = ~((id >> 1) ^ (id >> 3) ^ (id >> 4) ^ (id >> 5));
    
    return id | ((p0 & 1) << 6) | ((p1 & 1) << 7);
}

uint8_t calculateLINChecksum(uint8_t protected_id, uint8_t *data, size_t length) {
    uint16_t sum = protected_id;
    
    for(uint8_t i = 0; i < length; i++) {
        sum += data[i];
    }
    
    // RFC 1071 style checksum (Internet checksum)
    while(sum >> 8) {
        sum = (sum & 0xFF) + (sum >> 8);
    }
    
    return ~sum & 0xFF;
}

// Send complete LIN frame (master)
msg_t LinSendFrame(stLinFrame *frame, bool bChecksum, uint16_t nDelay) {
    systime_t timeout = chTimeMS2I(10);
    size_t size = 0;

    // 1. Send break field
    LinSendBreak();
    
    //chThdSleepMicroseconds(nDelay);

    // 2. Send sync byte
    uint8_t sync = 0x55;
    size = 1;
    if(uartSendFullTimeout(&UARTD2, &size, &sync, timeout) != MSG_OK) {
        return MSG_TIMEOUT;
    }

    //chThdSleepMicroseconds(nDelay);

    uint8_t txData[10];
    txData[0] = LinCalculateProtectedId(frame->nId);
    txData[1] = frame->nData[0]; // First data byte
    txData[2] = frame->nData[1]; // Second data byte
    txData[3] = frame->nData[2]; // Third data byte
    txData[4] = frame->nData[3]; // Fourth data byte
    txData[5] = frame->nData[4]; // Fifth data byte
    txData[6] = frame->nData[5]; // Sixth data byte
    txData[7] = frame->nData[6]; // Seventh data byte
    txData[8] = frame->nData[7]; // Eighth data byte
    if(bChecksum) {
        frame->nChecksum = calculateLINChecksum(txData[0], frame->nData, frame->nLength);
        txData[1 + frame->nLength] = frame->nChecksum;
        size = 2 + frame->nLength; // Sync + ID + Data + Checksum
    } else {
        size = 1 + frame->nLength; // Sync + ID + Data
    }

    if(uartSendFullTimeout(&UARTD2, &size, &txData, timeout) != MSG_OK) {
        return MSG_TIMEOUT;
    }

    return MSG_OK;
}

// LIN master thread
static THD_WORKING_AREA(lin_master_wa, 512);
static THD_FUNCTION(lin_master_thread, arg) {
    (void)arg;
    chRegSetThreadName("lin_master");

    stLinFrame frame;

    //ConfigureLinMode(&UARTD2);

    nCounter = 0;

    while (true) {
        
        //bSlowSpeed = bOn;

        if (nCounter > 15)
            nCounter = 0;

        // Send periodic frame
        frame.nId = 0x31;
        frame.nLength = 8;
        frame.nData[0] = (nCounter & 0x0F); //0x30 + (nCounter & 0x0F);
        frame.nData[1] = (bSlowSpeed << 7) | (bFastSpeed << 6) | (bIntMode << 5) | (bSwipeMode << 4) | (nIntModeSpeed & 0x0F);
        frame.nData[2] = 0x00;
        frame.nData[3] = 0x00;
        frame.nData[4] = 0x00;
        frame.nData[5] = 0x00;
        frame.nData[6] = 0x00;
        frame.nData[7] = 0x00;

        nCounter++;

        if (LinSendFrame(&frame, true, 5) == MSG_OK) {
            // Frame sent successfully
        }

        chThdSleepMilliseconds(500);

        /*
        frame.nId = 0x32;
        frame.nLength = 0;

        if (LinSendFrame(&frame, false, 1000) == MSG_OK) {
            // Frame sent successfully
        }

        chThdSleepMilliseconds(80);
        */

    }
}

void sendLINWakeupPulses(void) {
    // Send multiple dominant pulses to wake sleeping slaves
    USART_TypeDef *uart = UARTD2.usart;
    
    // Disable UART temporarily to control TX pin manually
    uartStop(&UARTD2);
    
    // Configure TX pin as GPIO output
    palSetLineMode(LINE_LIN_TX, PAL_MODE_OUTPUT_PUSHPULL); // Adjust pin for your setup
    
    // Send wake-up pulses: 5-10 dominant periods
    for(int i = 0; i < 8; i++) {
        palClearLine(LINE_LIN_TX);   // Dominant (12V)
        chThdSleepMicroseconds(500);  // 500µs dominant
        palSetLine(LINE_LIN_TX);    // Recessive (0V)
        chThdSleepMicroseconds(500);  // 500µs recessive
    }
    
    // Reconfigure pin back to UART
    palSetLineMode(LINE_LIN_TX, PAL_MODE_ALTERNATE(7)); // AF7 for USART

    // Re-enable UART
    uartStart(&UARTD2, &lin_config);

    // Wait for slaves to wake up
    chThdSleepMilliseconds(100);
}

void sendLINWakeupFrame(void) {
    // Send wake-up frame with PID 0x00
    size_t size;

    size = 1;
    uint8_t txData[1] = {0x00}; // PID 0x00
    systime_t timeout = chTimeMS2I(10);

    uartSendFullTimeout(&UARTD2, &size, &txData, timeout);
    
    // Wait for slaves to process wake-up
    chThdSleepMilliseconds(50);
}

void sendLINWakeupBreaks(void) {
    // Send multiple break fields to ensure wake-up
    for(int i = 0; i < 3; i++) {
        LinSendBreak();
        chThdSleepMicroseconds(1000);
    }
    
    // Wait for slaves to wake up
    chThdSleepMilliseconds(100);
}

// Put LIN transceiver to sleep
void LinSleep(void) {
    uartStop(&UARTD2);
    palClearLine(LINE_LIN_STANDBY); // Put TJA1029T to sleep
}

// Wake up LIN transceiver
void LinWakeup(void) {
    palSetLine(LINE_LIN_STANDBY);    // Wake up TJA1029T
    chThdSleepMilliseconds(1);       // Wait for wakeup
    //sendLINWakeupPulses();
    uartStart(&UARTD2, &lin_config); // Start UART driver
    //sendLINWakeupFrame(); // Send wake-up frame
    //sendLINWakeupBreaks(); // Send additional break fields
}


    

void InitLin() {
    //uartStart(&UARTD2, &lin_config); // Start UART driver

    LinWakeup();

    // Start LIN master thread
    chThdCreateStatic(lin_master_wa, sizeof(lin_master_wa), NORMALPRIO, lin_master_thread, nullptr);
    // Start LIN slave thread
    //chThdCreateStatic(lin_slave_wa, sizeof(lin_slave_wa), NORMALPRIO, lin_slave_thread, nullptr);
    
}