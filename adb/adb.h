#ifndef ADB_HEADER

#include "hardware/pio.h"

typedef void (*adb_handler_t)(uint8_t address, uint8_t reg, uint8_t *data, size_t len);

void initAdbPio(uint adb_pin, adb_handler_t handler);

bool adbReceivedServiceRequest();

void adbSendReset();
void adbFlush(uint8_t address);
void adbTalk(uint8_t address, uint8_t reg);
void adbListen(uint8_t address, uint8_t reg, uint8_t *data, uint8_t data_len);

#else
#define ADB_HEADER
#endif
