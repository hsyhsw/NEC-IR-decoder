#include <Arduino.h>

#include "uTimer.hpp"
#include "digitalWriteFast.h"

#define IR_INPUT_PIN PIND6

#define IR_THRESHOLD_LOW (192)
#define IR_THRESHOLD_HIGH (1024-IR_THRESHOLD_LOW)

#define SCOPE_DEBUG
#ifdef SCOPE_DEBUG
#define CH3_OUT_PIN A1
#define CH4_OUT_PIN A2
/**
 * blinky for time-bound polling.
 * viewable only through a scope. (see attached screenshot)
 */
#define BLINK_CH3() digitalWriteFast(CH3_OUT_PIN, HIGH); digitalWriteFast(CH3_OUT_PIN, LOW);
/**
 * blinky for bit recognition.
 * viewable only through a scope. (see attached screenshot)
 */
#define BLINK_CH4() digitalWriteFast(CH4_OUT_PIN, HIGH); digitalWriteFast(CH4_OUT_PIN, LOW);
#else
#define BLINK_CH3() asm volatile("nop\n nop\n")
#define BLINK_CH4() asm volatile("nop\n nop\n")
#endif

/**
 * N(on-)Mark: logical 0 (voltage VCC)
 */
bool isNMark() {
    return analogRead(IR_INPUT_PIN) > IR_THRESHOLD_HIGH;
}

/**
 * Mark: logical 1 (voltage GND)
 */
bool isMark() {
    return analogRead(IR_INPUT_PIN) < IR_THRESHOLD_LOW;
}

uTimer traceTimer;

/**
 * Wait for (logical) falling edge
 */
bool waitNMark(unsigned int window = 300) {
    traceTimer.start(window);
    BLINK_CH3();
    if (!isMark()) {
        return false;
    }
    while (isMark()) {
        if (traceTimer.expired()) {
            return false;
        }
    }
    return true;
}

/**
 * Wait for (logical) rising edge
 */
bool waitMark(unsigned int window = 300) {
    traceTimer.start(window);
    BLINK_CH3();
    if (!isNMark()) {
        return false;
    }
    while (isNMark()) {
        if (traceTimer.expired()) {
            return false;
        }
    }
    return true;
}

#define TIME_BOUNDED_LOOP(TIMER, T) TIMER.start(T); while (!TIMER.expired())

#define WAIT_MARK_OR_FAIL(DURATION) if (!waitMark(DURATION)) {return false;}
#define WAIT_NMARK_OR_FAIL(DURATION) if (!waitNMark(DURATION)) {return false;}
#define RETAIN_NMARK_OR_FAIL(DURATION) TIME_BOUNDED_LOOP(traceTimer, DURATION) {if (isMark()) {return false;}}
#define RETAIN_MARK_OR_FAIL(DURATION) TIME_BOUNDED_LOOP(traceTimer, DURATION) {if (isNMark()) {return false;}}

bool waitLeadingBurst() {
    // wait 9ms one, 4.5ms zero
    WAIT_MARK_OR_FAIL(900);
    RETAIN_MARK_OR_FAIL(8400);
    WAIT_NMARK_OR_FAIL(1000);
    RETAIN_NMARK_OR_FAIL(4100);
    return true;
}

bool consumeBurstMarkEdge() {
    WAIT_MARK_OR_FAIL(300);
    return true;
}

bool consumeBurstRemaining() {
    RETAIN_MARK_OR_FAIL(150);
    WAIT_NMARK_OR_FAIL(300);
    RETAIN_NMARK_OR_FAIL(200);
    return true;
}

#define CONSUME_BURST_OR_FAIL() if (!consumeBurstMarkEdge() || !consumeBurstRemaining()) {return false;}

inline bool checkIntegrity(const uint8_t v, const uint8_t vInv) {
    return (v ^ vInv) == 0xFF;
}

bool decodeStream(uint8_t *addr, uint8_t *data) {
    static uTimer streamTimer;

    uint32_t raw = 0;
    int idx = 31;

    streamTimer.start(58000); // 56ms + 2ms buffer

    CONSUME_BURST_OR_FAIL();
    // here we have 'space' or 'burst' after previous burst
    while (idx != -1 && !streamTimer.expired()) {
        // try consume trailing mark edge
        if (consumeBurstMarkEdge()) {
            // we have 'burst': 1
            raw |= (((uint32_t) 1) << idx--);
            BLINK_CH4();
            consumeBurstRemaining();
        } else {
            // we have 'space': 0
            --idx;
            BLINK_CH4();
            RETAIN_NMARK_OR_FAIL(700); // consume remaining 'space'
            CONSUME_BURST_OR_FAIL();
        }
    }

    Serial.print(streamTimer.elapsed() / 1000.0);
    Serial.println("ms spent for stream decoding.");
    Serial.println("raw bitstream: ");
    Serial.println(raw, 2);

    if (!checkIntegrity(raw >> 24, raw >> 16) || !checkIntegrity(raw >> 8, raw)) {
        return false;
    }
    *addr = raw >> 24;
    *data = raw >> 8;

    return true;
}

void setup() {
    Serial.begin(9600); // USB serial
    pinMode(IR_INPUT_PIN, INPUT);

#ifdef SCOPE_DEBUG
    pinMode(CH3_OUT_PIN, OUTPUT);
    pinMode(CH4_OUT_PIN, OUTPUT);
#endif
}

uint8_t address;
uint8_t data;

void loop() {
    if (!waitLeadingBurst()) {
        return;
    }

    address = 0x0;
    data = 0x0;
    if (decodeStream(&address, &data)) {
        Serial.print("Address: ");
        Serial.print(address, 16);
        Serial.println();
        Serial.print("Data   : ");
        Serial.print(data, 16);
        Serial.println();
    }

    delay(100);
}
