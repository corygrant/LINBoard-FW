#include "mailbox.h"
#include "ch.hpp"

//static chibios_rt::Mailbox<CANRxFrame*, MAILBOX_SIZE> rxMb;
//static chibios_rt::Mailbox<CANTxFrame*, MAILBOX_SIZE> txMb;
//static chibios_rt::Mailbox<CANTxFrame*, MAILBOX_SIZE> txUsbMb;

static mailbox_t rxMb;
static mailbox_t txMb;
static mailbox_t txUsbMb;

//Mailbox pointer buffers
static msg_t rxMbBuf[MAILBOX_SIZE];
static msg_t txMbBuf[MAILBOX_SIZE];
static msg_t txUsbMbBuf[MAILBOX_SIZE];

//Mailbox storage of CAN frames
//Not managed by mailbox
CANRxFrame rxFrames[MAILBOX_SIZE];
CANTxFrame txFrames[MAILBOX_SIZE];
CANTxFrame txUsbFrames[MAILBOX_SIZE];

//Used to manage the memory used by the mailbox
bool rxMsgUsed[MAILBOX_SIZE];
bool txMsgUsed[MAILBOX_SIZE];
bool txUsbMsgUsed[MAILBOX_SIZE];

void InitMailboxes()
{
    //Initialize the mailboxes
    chMBObjectInit(&rxMb, rxMbBuf, MAILBOX_SIZE);
    chMBObjectInit(&txMb, txMbBuf, MAILBOX_SIZE);
    chMBObjectInit(&txUsbMb, txUsbMbBuf, MAILBOX_SIZE);

    //Initialize the used arrays
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        rxMsgUsed[i] = false;
        txMsgUsed[i] = false;
        txUsbMsgUsed[i] = false;
    }
}

msg_t PostTxFrame(CANTxFrame *frame)
{
    PostTxUsbFrame(frame);  // Post to USB mailbox first

    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!txMsgUsed[i]) {
            // Find a free slot in can_messages[] to store the data
            txFrames[i] = *frame;
            txMsgUsed[i] = true;

            // Try to post the pointer to the mailbox
            msg_t result = chMBPostTimeout(&txMb, (msg_t)&txFrames[i], TIME_IMMEDIATE);
            if (result == MSG_OK) {
                return result;  // Successfully posted
            } else {
                txMsgUsed[i] = false;  // Free the slot if mailbox is full
                return result;
            }
        }
    }

    return MSG_TIMEOUT;  // No free slots
}

msg_t FetchTxFrame(CANTxFrame *frame)
{
    CANTxFrame *txFrame;
    // Fetch a pointer from the mailbox
    msg_t result = chMBFetchTimeout(&txMb, (msg_t*)&txFrame, TIME_IMMEDIATE);
    if (result == MSG_OK) {
        // Mark the slot in memory as free
        for (int i = 0; i < MAILBOX_SIZE; i++) {
            if (txFrame == &txFrames[i]) {
                txMsgUsed[i] = false;
                break;
            }
        }
        *frame = *txFrame;
        return result;
    }
    return result;
}

msg_t PostTxUsbFrame(CANTxFrame *frame)
{
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!txUsbMsgUsed[i]) {
            // Find a free slot in can_messages[] to store the data
            txUsbFrames[i] = *frame;
            txUsbMsgUsed[i] = true;

            // Try to post the pointer to the mailbox
            msg_t result = chMBPostTimeout(&txUsbMb, (msg_t)&txUsbFrames[i], TIME_IMMEDIATE);
            if (result == MSG_OK) {
                return result;  // Successfully posted
            } else {
                txUsbMsgUsed[i] = false;  // Free the slot if mailbox is full
                return result;
            }
        }
    }

    return MSG_TIMEOUT;  // No free slots
}

msg_t FetchTxUsbFrame(CANTxFrame *frame)
{
    CANTxFrame *txFrame;
    // Fetch a pointer from the mailbox
    msg_t result = chMBFetchTimeout(&txUsbMb, (msg_t*)&txFrame, TIME_IMMEDIATE);
    if (result == MSG_OK) {
        // Mark the slot in memory as free
        for (int i = 0; i < MAILBOX_SIZE; i++) {
            if (txFrame == &txUsbFrames[i]) {
                txUsbMsgUsed[i] = false;
                break;
            }
        }
        *frame = *txFrame;
        return result;
    }
    return result;
}

msg_t PostRxFrame(CANRxFrame *frame)
{
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!rxMsgUsed[i]) {
            // Find a free slot in can_messages[] to store the data
            rxFrames[i] = *frame;
            rxMsgUsed[i] = true;

            // Try to post the pointer to the mailbox
            msg_t result = chMBPostTimeout(&rxMb, (msg_t)&rxFrames[i], TIME_IMMEDIATE);
            if (result == MSG_OK) {
                return result;  // Successfully posted
            } else {
                rxMsgUsed[i] = false;  // Free the slot if mailbox is full
                return result;
            }
        }
    }

    return MSG_TIMEOUT;  // No free slots
}

msg_t FetchRxFrame(CANRxFrame *frame)
{
    CANRxFrame *rxFrame;
    // Fetch a pointer from the mailbox
    msg_t result = chMBFetchTimeout(&rxMb, (msg_t*)&rxFrame, TIME_IMMEDIATE);
    if (result == MSG_OK) {
        // Mark the slot in memory as free
        for (int i = 0; i < MAILBOX_SIZE; i++) {
            if (rxFrame == &rxFrames[i]) {
                rxMsgUsed[i] = false;
                break;
            }
        }
        *frame = *rxFrame;
        return result;
    }
    return result;
}

bool RxFramesEmpty()
{
    return (rxMb.cnt == 0);
}