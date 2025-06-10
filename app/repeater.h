/* Copyright 2025, Licensed under the Apache License, Version 2.0 */

#ifndef APP_REPEATER_H
#define APP_REPEATER_H

#include <stdbool.h>
#include <stdint.h>

void REPEATER_Init(void);
void REPEATER_SetPermanentStandby(bool enable);
bool REPEATER_GetPermanentStandby(void);
void REPEATER_Process(void);
void REPEATER_WakeFromStandby(void);
void REPEATER_ProcessTapCommand(uint8_t taps);

#endif