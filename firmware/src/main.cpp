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

// On-board WS2812B status LED (blue pad on the SuperMini).
static const int LED_PIN = 48;

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

// --- status LED helpers ---------------------------------------------------

// Set the on-board WS2812B to a given RGB colour. Values are 0..255 per channel.
static void ledSet(uint8_t r, uint8_t g, uint8_t b) {
    neopixelWrite(LED_PIN, r, g, b);
}

// Purple while transmitting - looks intentional, not just "on".
static const uint8_t LED_R_ACTIVE = 160;
static const uint8_t LED_G_ACTIVE =  40;
static const uint8_t LED_B_ACTIVE = 255;

// Soft red for "done" - visible but not harsh.
static const uint8_t LED_R_DONE = 220;
static const uint8_t LED_G_DONE =  30;
static const uint8_t LED_B_DONE =  30;

// Fade the LED to black over ~400 ms so it doesn't just snap off.
static void ledFadeOut() {
    for (int i = 10; i >= 0; i--) {
        int scale = i * 25;   // 250 .. 0
        ledSet((LED_R_DONE * scale) / 255,
               (LED_G_DONE * scale) / 255,
               (LED_B_DONE * scale) / 255);
        delay(40);
    }
    ledSet(0, 0, 0);
}

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
    // Purple while transmitting - "I'm doing my thing".
    ledSet(LED_R_ACTIVE, LED_G_ACTIVE, LED_B_ACTIVE);

    if (irsend) {
        irsend->enableIROut(38000);   // warm up the RMT driver with a default carrier
    }
    runAll();

    // All codes sent. Flash to red briefly so you can see it finished, then fade out.
    ledSet(LED_R_DONE, LED_G_DONE, LED_B_DONE);
    delay(800);
    ledFadeOut();

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
