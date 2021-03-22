#ifndef ADB_HEADER

#include "hardware/pio.h"

void initAdbPio(PIO pio, uint adb_sm, uint adb_pin);

bool adbReceivedServiceRequest(PIO pio);

void adbSendReset(PIO pio, uint sm);
void adbFlush(PIO pio, uint sm, uint8_t address);
uint8_t adbTalk(PIO pio, uint sm, uint8_t address, uint8_t reg, uint8_t *data);
void adbListen(PIO pio, uint sm, uint8_t address, uint8_t reg, uint8_t *data, uint8_t data_len);

#else
#define ADB_HEADER
#endif
