#include "lin.h"
#include "port.h"
#include <cstring>

uint8_t nCounter = 0;

bool bSlowSpeed = true;
bool bFastSpeed = false;
bool bIntMode = false;
bool bSwipeMode = false;
uint8_t nIntModeSpeed = 1;

static void ConfigureLinMode(SerialDriver *sdp) {
    USART_TypeDef *uart = sdp->usart;
    
    // Disable USART first
    uart->CR1 &= ~USART_CR1_UE;
    
    // Configure CR1 register
    uint32_t cr1 = uart->CR1;
    cr1 &= ~(USART_CR1_M0);    // Clear M bits for 8-bit word length
    uart->CR1 = cr1;
    
    // Configure CR2 register for LIN mode
    uint32_t cr2 = uart->CR2;
    cr2 |= USART_CR2_LINEN;                   // Set LINEN bit to enter LIN mode
    cr2 &= ~(USART_CR2_STOP_0 |               // Clear STOP bits (required for LIN)
             USART_CR2_STOP_1 |
             USART_CR2_CLKEN);                // Clear CLKEN bit (required for LIN)
    uart->CR2 = cr2;
    
    // Configure CR3 register - clear required bits for LIN
    uint32_t cr3 = uart->CR3;
    cr3 &= ~(USART_CR3_SCEN |                 // Clear Smart Card mode
             USART_CR3_HDSEL |                // Clear Half-duplex mode
             USART_CR3_IREN);                 // Clear IrDA mode
    uart->CR3 = cr3;
    
    // Re-enable USART
    uart->CR1 |= USART_CR1_UE;
}

// Generate LIN break using baud rate switching
static void LinSendBreak(void) {
    // Stop current configuration
    sdStop(&SD2);
    
    // Configure for break generation (half baud rate)
    SerialConfig break_config = {
        9600,  // Half baud rate
        0, 0, 0
    };
    sdStart(&SD2, &break_config);
    
    // Send break byte
    uint8_t break_byte = 0x00;
    sdWrite(&SD2, &break_byte, 1);
    
    // Wait for transmission to complete
    chThdSleepMicroseconds(LIN_BREAK_DURATION_US);
    
    // Restore normal configuration
    sdStop(&SD2);
    sdStart(&SD2, &linConfig);
}

// Alternative break generation using GPIO
static void LinSendBreakGpio(void) {
    // Switch TX pin to GPIO mode
    palSetPadMode(GPIOA, 2, PAL_MODE_OUTPUT_PUSHPULL);
    palClearPad(GPIOA, 2);  // Pull low for break

    chThdSleepMicroseconds(LIN_BREAK_DURATION_US);

    palSetPad(GPIOA, 2);  // Release line

    chThdSleepMicroseconds(100);  // Short delay after break

    // Restore UART mode
    palSetPadMode(GPIOA, 2, PAL_MODE_ALTERNATE(7));
}

static void LinSendBreakSTM32(void)
{
    SD2.usart->RQR |= USART_RQR_SBKRQ; // Send break

    //chThdSleepMicroseconds(500);  // Short delay after break

    while(SD2.usart->ISR & USART_ISR_SBKF); // Wait until break is sent
}

// Calculate protected ID with parity bits
static uint8_t LinCalculateProtectedId(uint8_t id) {
    uint8_t p0 = (id >> 0) ^ (id >> 1) ^ (id >> 2) ^ (id >> 4);
    uint8_t p1 = ~((id >> 1) ^ (id >> 3) ^ (id >> 4) ^ (id >> 5));
    
    return id | ((p0 & 1) << 6) | ((p1 & 1) << 7);
}

// Calculate enhanced checksum (LIN 2.x)
static uint8_t LinCalculateChecksum(uint8_t protected_id, uint8_t *data, uint8_t length) {
    uint16_t sum = protected_id;
    
    for (uint8_t i = 0; i < length; i++) {
        sum += data[i];
        if (sum > 0xFF) {
            sum = (sum & 0xFF) + 1;  // Add carry
        }
    }
    
    return (~sum) & 0xFF;
}

// Send complete LIN frame (master)
msg_t LinSendFrame(stLinFrame *frame, bool bChecksum) {
    systime_t timeout = chTimeMS2I(100);
    
    // 1. Send break field
    LinSendBreakSTM32();
    
    chThdSleepMicroseconds(500);

    // 2. Send sync byte
    uint8_t sync = LIN_SYNC_BYTE;
    if (sdWriteTimeout(&SD2, &sync, 1, timeout) != 1) {
        return MSG_TIMEOUT;
    }

    chThdSleepMicroseconds(500);
    
    // 3. Send protected identifier
    uint8_t protected_id = LinCalculateProtectedId(frame->nId);
    if (sdWriteTimeout(&SD2, &protected_id, 1, timeout) != 1) {
        return MSG_TIMEOUT;
    }
    
    // Small delay for interbyte spacing
    //chThdSleepMicroseconds(1000);
    
    if (frame->nLength != 0){
        for(uint8_t i = 0; i < frame->nLength; i++) {
            // 4. Send data bytes
            if (sdWriteTimeout(&SD2, &frame->nData[i], 1, timeout) != 1) {
                return MSG_TIMEOUT;
            }

            //chThdSleepMicroseconds(1000);
        }
    }
    else
    {
        //chThdSleepMicroseconds(100);
    }

    
    if(bChecksum)
    {
        // 5. Calculate and send checksum
        frame->nChecksum = LinCalculateChecksum(protected_id, frame->nData, frame->nLength);
        if (sdWriteTimeout(&SD2, &frame->nChecksum, 1, timeout) != 1) {
            return MSG_TIMEOUT;
        }
    }
    
    return MSG_OK;
}

// Send header and receive response (master requesting data from slave)
msg_t LinReceiveResponse(uint8_t id, stLinFrame *frame, systime_t timeout) {
    // Send header only
    LinSendBreakGpio();

    uint8_t sync = LIN_SYNC_BYTE;
    if (sdWriteTimeout(&SD2, &sync, 1, chTimeMS2I(100)) != 1) {
        return MSG_TIMEOUT;
    }

    uint8_t protected_id = LinCalculateProtectedId(id);
    if (sdWriteTimeout(&SD2, &protected_id, 1, chTimeMS2I(100)) != 1) {
        return MSG_TIMEOUT;
    }
    
    // Wait for slave response
    uint8_t rx_buffer[9];  // Max 8 data + checksum
    size_t bytes_to_read = frame->nLength + 1;  // data + checksum
    
    if (sdReadTimeout(&SD2, rx_buffer, bytes_to_read, timeout) != bytes_to_read) {
        return MSG_TIMEOUT;
    }
    
    // Copy data and verify checksum
    memcpy(frame->nData, rx_buffer, frame->nLength);
    uint8_t expected_checksum = LinCalculateChecksum(protected_id, frame->nData, frame->nLength);

    if (expected_checksum != rx_buffer[frame->nLength]) {
        return MSG_RESET;  // Checksum error
    }

    frame->nId = id;
    frame->nChecksum = rx_buffer[frame->nLength];
    
    return MSG_OK;
}

// LIN slave response handler (responds to received headers)
msg_t LinSlaveHandler(systime_t timeout) {
    uint8_t rx_byte;
    
    while (true) {
        // Wait for break detection (sync byte after break)
        if (sdReadTimeout(&SD2, &rx_byte, 1, timeout) != 1) {
            return MSG_TIMEOUT;
        }
        
        if (rx_byte == LIN_SYNC_BYTE) {
            // Read protected ID
            if (sdReadTimeout(&SD2, &rx_byte, 1, chTimeMS2I(10)) != 1) {
                continue;
            }
            
            uint8_t id = rx_byte & 0x3F;  // Extract ID (remove parity bits)
            /*
            // Handle based on ID
            switch (id) {
                case 0x01: {
                    // Respond with data
                    stLinFrame response;
                    response.nId = id;
                    response.nLength = 4;
                    response.nData[0] = 0x12;
                    response.nData[1] = 0x34;
                    response.nData[2] = 0x56;
                    response.nData[3] = 0x78;

                    response.nChecksum = LinCalculateChecksum(rx_byte, response.nData, response.nLength);

                    sdWriteTimeout(&SD2, response.nData, response.nLength, chTimeMS2I(50));
                    sdWriteTimeout(&SD2, &response.nChecksum, 1, chTimeMS2I(10));
                    break;
                }
                
                case 0x02: {
                    // Receive data from master
                    uint8_t data[9];
                    // Read expected data length + checksum
                    if (sdReadTimeout(&SD2, data, 3, chTimeMS2I(50)) == 3) {
                        // Process received data
                        // data[0], data[1] are payload, data[2] is checksum
                    }
                    break;
                }
            }
                */
        }
    }
}

// LIN master thread
static THD_WORKING_AREA(lin_master_wa, 512);
static THD_FUNCTION(lin_master_thread, arg) {
    (void)arg;
    chRegSetThreadName("lin_master");

    stLinFrame frame;

    ConfigureLinMode(&SD2);

    nCounter = 0;

    while (true) {
        
        if (nCounter > 15)
            nCounter = 0;

        // Send periodic frame
        frame.nId = 0x31;
        frame.nLength = 8;
        frame.nData[0] = 0x30 + (nCounter & 0x0F);
        frame.nData[1] = (bSlowSpeed << 7) | (bFastSpeed << 6) | (bIntMode << 5) | (bSwipeMode << 4) | (nIntModeSpeed & 0x0F);
        frame.nData[2] = 0x00;
        frame.nData[3] = 0x00;
        frame.nData[4] = 0x00;
        frame.nData[5] = 0x00;
        frame.nData[6] = 0x00;
        frame.nData[7] = 0x00;

        nCounter++;

        if (LinSendFrame(&frame, true) == MSG_OK) {
            // Frame sent successfully
        }

        //chThdSleepMilliseconds(5);

        //frame.nId = nCounter;
        //frame.nLength = 0;

        if (LinSendFrame(&frame, false) == MSG_OK) {
            // Frame sent successfully
        }

        chThdSleepMilliseconds(100);
        
        // Request data from slave
        //frame.nId = 0x20;
        //frame.nLength = 4;

        //if (LinReceiveResponse(0x20, &frame, chTimeMS2I(1000)) == MSG_OK) {
            // Process received data
            // frame.nData[] contains the response
        //}
        
        //chThdSleepMilliseconds(1000);
    }
}

// LIN slave thread
static THD_WORKING_AREA(lin_slave_wa, 512);
static THD_FUNCTION(lin_slave_thread, arg) {
    (void)arg;
    chRegSetThreadName("lin_slave");
    
    while (true) {
        LinSlaveHandler(chTimeMS2I(5000));
        chThdSleepMilliseconds(10);
    }
}


// Put LIN transceiver to sleep
void LinSleep(void) {
    sdStop(&SD2);
    palClearLine(LINE_LIN_STANDBY); // Put TJA1029T to sleep
}

// Wake up LIN transceiver
void LinWakeup(void) {
    palSetLine(LINE_LIN_STANDBY); // Wake up TJA1029T
    chThdSleepMilliseconds(10);                   // Wait for wakeup
    sdStart(&SD2, &linConfig);
}


void InitLin() {
    // Initialize serial driver
    sdStart(&SD2, &linConfig);
    
    LinWakeup();

    // Start LIN master thread
    chThdCreateStatic(lin_master_wa, sizeof(lin_master_wa), NORMALPRIO, lin_master_thread, nullptr);
    // Start LIN slave thread
    //chThdCreateStatic(lin_slave_wa, sizeof(lin_slave_wa), NORMALPRIO, lin_slave_thread, nullptr);
    
}