#ifndef _LOGIC_HANDLE_
#define _LOGIC_HANDLE_
#include <stdint.h>
#include <stdbool.h>
#include "Datamanager.h"
#include <ctype.h>


uint8_t charArray_to_uint8(char *arr, uint8_t length);
uint8_t hexDigitToValue(char c);
bool containsASCII(const char *dataInput, const char *chars);
void convertCharToHex(uint8_t *arrayOutPut, const char *arrayInPut, uint8_t length);
void readSensorData(dm_telemetry_t *telemetry);
#endif // _LOGIC_HANDLE_