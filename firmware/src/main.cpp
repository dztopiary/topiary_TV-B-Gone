/*
 * TV-B-GONE - Minimal one-shot ESP32-S3 SuperMini firmware
 * --------------------------------------------------------
 * Blasts universal power-off IR codes at TVs and projectors.
 *
 * Region: EU (Polska/Europe). The NA code bank from WORLD_IR_CODES.h is not
 * sent - it would only add ~3 s of transmission for devices outside Europe.
 *
 * Hardware (minimal):
 *   - ESP32-S3 SuperMini
 *   - High-power 940 nm IR LED module (1W / 3W) -> 5V / GND / Signal
 *     Wire the Signal pin to GPIO_IR below.
 *   - Battery + a simple on/off power switch feeding the ESP32-S3's Vin/3V3.
 *   - No buttons, no display, no SD card.
 *
 * Operation:
 *   Flip the power switch ON -> firmware boots and immediately sends every
 *   EU power code plus the universal parsed + raw sets (TVs AND projectors).
 *   The full blast takes ~5-7 s. When it finishes, the ESP32 enters deep
 *   sleep (near-zero draw) so the battery lasts; flip the switch OFF then ON
 *   to run it again. There is no infinite loop and nothing else to press.
 */

#include <Arduino.h>
#include <IRsend.h>          // IRremoteESP8266
#include "WORLD_IR_CODES.h"  // local copy of the code DB

// ----------------------------- Pin map ------------------------------------
static const int GPIO_IR = 4;   // D4 -> IR LED module signal pin

// ----------------------------- Timing -------------------------------------
static const uint32_t CODE_GAP_US = 20500;   // ~20.5 ms gap between codes (same as Bruce)

static IRsend *irsend;   // initialized in setup() once the pin is known

// Bit-compression state used while decoding parsed codes (mirrors Bruce).
static uint8_t bitsleft_r = 0;
static uint8_t bits_r     = 0;
static uint8_t code_ptr   = 0;
static volatile const IrCode *powerCode = nullptr;

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

// ----------------------------- TX helpers ---------------------------------
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
            rawData[k * 2]       = powerCode->times[ti] * 10;     // x10 -> microseconds
            rawData[(k * 2) + 1] = powerCode->times[ti + 1] * 10;
        }

        irsend->sendRaw(rawData, numpairs * 2, freq);
        bitsleft_r = 0;
        delayMicroseconds(CODE_GAP_US);
    }
}

static void sendRawBatch(const RawIrCode *const *codes, uint8_t count) {
    static uint16_t rawData[300];
    for (uint8_t i = 0; i < count; i++) {
        const RawIrCode *rc = codes[i];
        const uint8_t freq     = rc->timer_val;
        const uint8_t numpairs = rc->numpairs;

        for (uint8_t k = 0; k < numpairs; k++) {
            rawData[k * 2]       = (uint16_t)rc->times[k * 2];      // already in microseconds
            rawData[(k * 2) + 1] = (uint16_t)rc->times[(k * 2) + 1];
        }

        irsend->sendRaw(rawData, numpairs * 2, freq);
        delayMicroseconds(CODE_GAP_US);
    }
}

static void runAll() {
    // EU power-off codes first (Polska/Europe), then the universal parsed set
    // (Samsung/LG/NEC/RCA/Pioneer + LG-projector / NEC-projector variants),
    // then the small raw set that adds projector on/off/menu, generic "most
    // TVs", Xiaomi etc. for any device that only responds to a raw burst.
    sendParsedBatch(EUpowerCodes, num_EUcodes);
    sendParsedBatch(UniversalParsedCodes, num_UniversalParsedCodes);
    sendRawBatch(UniversalRawCodes, num_UniversalRawCodes);
}

// ----------------------------- Setup / Loop --------------------------------
void setup() {
    pinMode(GPIO_IR, OUTPUT);
    digitalWrite(GPIO_IR, LOW);
    irsend = new IRsend((uint16_t)GPIO_IR);
    // No need to call enableIROut(): sendRaw(freq) sets the carrier per code.

    runAll();
}

// One-shot: after the blast completes, park in deep sleep until power is cut
// (or re-applied). This keeps battery draw ~0 while "done" and gives you a
// clean reset path: flip the switch OFF then ON to run again.
void loop() {
    esp_deep_sleep_start();   // never returns; we're done for this power cycle
}
