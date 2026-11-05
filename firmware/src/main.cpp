/*
 * TV-B-Gone for the ESP32-S3 SuperMini.
 *
 * Flip the power switch and it fires every IR "power off" code I could find,
 * then goes to sleep. Point it at a room full of TVs/projectors and press
 * once. No buttons, no screen - just battery + switch + one IR LED.
 *
 * The NA code set in WORLD_IR_CODES.h is left out on purpose: I'm in Europe
 * so those would only add a few seconds for nothing.
 */

#include <Arduino.h>
#include <IRsend.h>          // IRremoteESP8266
#include "WORLD_IR_CODES.h"  // the code database

// The signal pin on the IR LED module goes here (D4).
static const int GPIO_IR = 4;

// Pause between codes. Same value Bruce uses - leaves enough room for slow
// receivers to latch each burst before the next one starts.
static const uint32_t CODE_GAP_US = 20500;   // ~20 ms

// Built in setup() once we know which pin it's on.
static IRsend *irsend;

// State for expanding the bit-compressed code entries (copied from Bruce).
static uint8_t bitsleft_r = 0;
static uint8_t bits_r     = 0;
static uint8_t code_ptr   = 0;
static volatile const IrCode *powerCode = nullptr;

// Pull `count` bits out of the current compressed code, MSB first.
static uint8_t read_bits(uint8_t count) {
    uint8_t tmp = 0;
    while (count--) {
        if (bitsleft_r == 0) {
            bits_r     = powerCode->codes[code_ptr++];
            bitsleft_r = 8;
        }
        tmp = (tmp << 1) | ((bits_r >> --bitsleft_r) & 1);
    }
    return tmp;
}

// Expand the "parsed" codes. The stored times are in units of ~10us, so we
// multiply by 10 to get real microseconds before sending.
static void sendParsedBatch(const IrCode *const *codes, uint8_t count) {
    static uint16_t rawData[300];
    for (uint8_t i = 0; i < count; i++) {
        powerCode = codes[i];
        const uint8_t freq           = powerCode->timer_val;
        const uint8_t numpairs       = powerCode->numpairs;
        const uint8_t bitcompression = powerCode->bitcompression;
        code_ptr                     = 0;

        for (uint8_t k = 0; k < numpairs; k++) {
            uint16_t ti = (read_bits(bitcompression)) * 2;
            rawData[k * 2]       = powerCode->times[ti] * 10;
            rawData[(k * 2) + 1] = powerCode->times[ti + 1] * 10;
        }

        irsend->sendRaw(rawData, numpairs * 2, freq);
        bitsleft_r = 0;
        delayMicroseconds(CODE_GAP_US);
    }
}

// Same idea but these entries already store raw microsecond timings.
static void sendRawBatch(const RawIrCode *const *codes, uint8_t count) {
    static uint16_t rawData[300];
    for (uint8_t i = 0; i < count; i++) {
        const RawIrCode *rc = codes[i];
        const uint8_t freq     = rc->timer_val;
        const uint8_t numpairs = rc->numpairs;

        for (uint8_t k = 0; k < numpairs; k++) {
            rawData[k * 2]       = (uint16_t)rc->times[k * 2];
            rawData[(k * 2) + 1] = (uint16_t)rc->times[(k * 2) + 1];
        }

        irsend->sendRaw(rawData, numpairs * 2, freq);
        delayMicroseconds(CODE_GAP_US);
    }
}

// Fire everything: EU set first (closest to what we'll actually hit), then the
// universal parsed codes (TVs + a few projector variants), then the small raw
// bucket that catches strays like generic "most TVs" and Xiaomi.
static void runAll() {
    sendParsedBatch(EUpowerCodes, num_EUcodes);
    sendParsedBatch(UniversalParsedCodes, num_UniversalParsedCodes);
    sendRawBatch(UniversalRawCodes, num_UniversalRawCodes);
}

// One-shot: fire all codes, then drop into deep sleep. Draw goes basically to
// zero so the battery just sits there until you flip the switch off/on again.
static void runBlastAndSleep() {
    if (irsend) {
        irsend->enableIROut(38000);   // warm up the RMT driver with a default carrier
    }
    runAll();
    esp_deep_sleep_start();           // no wake source, so it stays asleep until power cycle
}

void setup() {
    pinMode(GPIO_IR, OUTPUT);
    digitalWrite(GPIO_IR, LOW);
    irsend = new IRsend((uint16_t)GPIO_IR);   // sendRaw sets the carrier per code

    runBlastAndSleep();
}

// Never actually reached - deep sleep keeps us here. Kept so the Arduino sketch
// contract still has a loop().
void loop() {
    esp_deep_sleep_start();
}
