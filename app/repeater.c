/* Copyright 2025, Licensed under the Apache License, Version 2.0 */

#include "app/repeater.h"
#include "app/app.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "driver/uart.h"
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include <string.h>
#include <stdio.h>

typedef enum {
    REPEATER_STATE_IDLE,
    REPEATER_STATE_STANDBY,
    REPEATER_STATE_WAKE,
    REPEATER_STATE_MORSE,
    REPEATER_STATE_LISTEN,
    REPEATER_STATE_ACTIVE
} RepeaterState_t;

static RepeaterState_t gRepeaterState = REPEATER_STATE_IDLE;
static uint32_t gNextWakeTime = 0;
static uint32_t gStateTimer = 0;
static uint32_t gWatchdogTimer = 0;
static bool gPermanentStandby = false;
static uint8_t gTapCommand = 0;

void REPEATER_Init(void) {
    uint8_t data;
    EEPROM_ReadBuffer(0x1F00, &data, 1);
    gPermanentStandby = (data & 0x01) != 0;
    gRepeaterState = gPermanentStandby ? REPEATER_STATE_STANDBY : REPEATER_STATE_IDLE;
    if (gPermanentStandby) {
        DebugPrint("Permanent Standby: Enabled");
        BK4819_Idle();
        ST7565_Sleep();
        PCON |= (1 << 1); // Enter STOP mode
    } else {
        DebugPrint("Permanent Standby: Disabled");
    }
    gNextWakeTime = SYSTEM_GetSysTick() + (4 * 60 * 60 * 100); // 4 hours in 10ms ticks
}

void REPEATER_SetPermanentStandby(bool enable) {
    gPermanentStandby = enable;
    uint8_t data = enable ? 0x01 : 0x00;
    EEPROM_WriteBuffer(0x1F00, &data, 1);
    if (enable && gRepeaterState != REPEATER_STATE_STANDBY) {
        gRepeaterState = REPEATER_STATE_STANDBY;
        DebugPrint("Entering Standby");
        BK4819_Idle();
        ST7565_Sleep();
        PCON |= (1 << 1);
    } else if (!enable && gRepeaterState == REPEATER_STATE_STANDBY) {
        gRepeaterState = REPEATER_STATE_IDLE;
        DebugPrint("Exiting Standby");
        RADIO_SetupRegisters(true);
    }
}

bool REPEATER_GetPermanentStandby(void) {
    return gPermanentStandby;
}

void TransmitMorseID(void) {
    // Placeholder: Implement Morse code transmission
    DebugPrint("Transmitting Morse ID");
    SYSTEM_DelayMs(10000); // Simulate 10s transmission
}

void SwitchFrequency(void) {
    // Placeholder: Implement frequency switch based on your DTMF detection
    DebugPrint("Switching Frequency");
    RADIO_SetupRegisters(true);
}

void REPEATER_ProcessTapCommand(uint8_t taps) {
    char buf[32];
    if (taps >= 3 && taps <= 5) {
        gTapCommand = taps;
        snprintf(buf, sizeof(buf), "Tap command: %d", taps);
        DebugPrint(buf);
        switch (taps) {
            case 3: DebugPrint("ESP32: Command 3"); break;
            case 4: DebugPrint("ESP32: Command 4"); break;
            case 5: DebugPrint("ESP32: Command 5"); break;
        }
    } else {
        gTapCommand = 0;
    }
}

void REPEATER_WakeFromStandby(void) {
    if (gRepeaterState == REPEATER_STATE_STANDBY) {
        gRepeaterState = REPEATER_STATE_WAKE;
        gStateTimer = SYSTEM_GetSysTick();
        gWatchdogTimer = gStateTimer;
        DebugPrint("Waking from Standby");
        RADIO_SetupRegisters(true);
        gRxIdleMode = false;
    }
}

static void EnterStandby(void) {
    gRepeaterState = REPEATER_STATE_STANDBY;
    DebugPrint("Entering Standby");
    BK4819_Idle();
    ST7565_Sleep();
    PCON |= (1 << 1);
}

void REPEATER_Process(void) {
    if (gCurrentFunction == FUNCTION_TRANSMIT || gCurrentFunction == FUNCTION_RECEIVE) {
        return; // Skip repeater logic during TX/RX
    }

    uint32_t now = SYSTEM_GetSysTick();
    char buf[32];

    // Watchdog: Reset if no activity for 5 seconds
    if (gRepeaterState != REPEATER_STATE_STANDBY && (now - gWatchdogTimer > 500)) {
        gRepeaterState = gPermanentStandby ? REPEATER_STATE_STANDBY : REPEATER_STATE_IDLE;
        gStateTimer = now;
        gWatchdogTimer = now;
        DebugPrint("Watchdog timeout");
        if (gPermanentStandby) {
            EnterStandby();
        }
        return;
    }

    switch (gRepeaterState) {
        case REPEATER_STATE_IDLE:
            if (gPermanentStandby) {
                EnterStandby();
            } else if (now >= gNextWakeTime) {
                gRepeaterState = REPEATER_STATE_WAKE;
                gStateTimer = now;
                gWatchdogTimer = now;
                DebugPrint("Scheduled wake");
                RADIO_SetupRegisters(true);
                gRxIdleMode = false;
            }
            break;

        case REPEATER_STATE_STANDBY:
            // Handled by PTT wake or scheduled wake
            if (now >= gNextWakeTime) {
                gRepeaterState = REPEATER_STATE_WAKE;
                gStateTimer = now;
                gWatchdogTimer = now;
                DebugPrint("Scheduled wake from Standby");
                RADIO_SetupRegisters(true);
                gRxIdleMode = false;
            }
            break;

        case REPEATER_STATE_WAKE:
            if (now - gStateTimer >= 100) { // 1s setup time
                gRepeaterState = REPEATER_STATE_MORSE;
                gStateTimer = now;
                gWatchdogTimer = now;
                DebugPrint("State: Morse");
            }
            break;

        case REPEATER_STATE_MORSE:
            TransmitMorseID();
            gRepeaterState = REPEATER_STATE_LISTEN;
            gStateTimer = now;
            gWatchdogTimer = now;
            DebugPrint("State: Listen");
            RADIO_SetupRegisters(true);
            break;

        case REPEATER_STATE_LISTEN:
            if (now - gStateTimer >= 5000) { // 50s listen
                gNextWakeTime = now + (4 * 60 * 60 * 100); // Next wake in 4 hours
                gRepeaterState = gPermanentStandby ? REPEATER_STATE_STANDBY : REPEATER_STATE_IDLE;
                gStateTimer = now;
                gWatchdogTimer = now;
                snprintf(buf, sizeof(buf), "Next wake: %lu", gNextWakeTime);
                DebugPrint(buf);
                if (gPermanentStandby) {
                    EnterStandby();
                }
            }
            break;

        case REPEATER_STATE_ACTIVE:
            // Placeholder for active repeater mode
            gWatchdogTimer = now;
            break;
    }
}