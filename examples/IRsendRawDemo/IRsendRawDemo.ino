/*
 * IRremote: IRsendRawDemo - demonstrates sending IR codes with sendRaw
 * An IR LED must be connected to Arduino PWM pin 3.
 * Initially coded 2009 Ken Shirriff http://www.righto.com
 *
 * IRsendRawDemo - added by AnalysIR (via www.AnalysIR.com), 24 August 2015
 *
 * This example shows how to send a RAW signal using the IRremote library.
 * The example signal is actually a 32 bit NEC signal.
 * Remote Control button: LGTV Power On/Off.
 * Hex Value: 0x20DF10EF, 32 bits
 *
 * It is more efficient to use the sendNEC function to send NEC signals.
 * Use of sendRaw here, serves only as an example of using the function.
 *
 */

#include <IRremote.h>

IRsend IrSender;

// On the Zero and others we switch explicitly to SerialUSB
#if defined(ARDUINO_ARCH_SAMD)
#define Serial SerialUSB
#endif

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);
#if defined(__AVR_ATmega32U4__) || defined(SERIAL_USB) || defined(SERIAL_PORT_USBVIRTUAL)
    delay(2000); // To be able to connect Serial monitor after reset and before first printout
#endif
    // Just to know which program is running on my Arduino
    Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRREMOTE));
    Serial.print(F("Ready to send IR signals at pin "));
    Serial.println(IR_SEND_PIN);
}

/*
 * NEC address=0xFB0C, command=0x18
 *
 * This is data in byte format.
 * The uint8_t/byte elements contain the number of ticks in 50 us.
 * The uint16_t format contains the (number of ticks * 50) if generated by IRremote,
 * so the uint16_t format has exact the same resolution but requires double space.
 * With the uint16_t format, you are able to modify the timings to meet the standards,
 * e.g. use 560 (instead of 11 * 50) for NEC or use 432 for Panasonic. But in this cases,
 * you better use the timing generation functions e.g. sendNECStandard() directly.
 */
const uint8_t irSignalP[] PROGMEM
= { 180, 90 /*Start bit*/, 11, 11, 11, 11, 11, 34, 11, 34/*0011 0xC of address LSB first*/, 11, 11, 11, 11, 11, 11, 11, 11/*0000*/,
        11, 34, 11, 34, 11, 11, 11, 34/*1101 0xB*/, 11, 34, 11, 34, 11, 34, 11, 34/*1111*/, 11, 11, 11, 11, 11, 11, 11,
        34/*0001 0x08 of command LSB first*/, 11, 34, 11, 11, 11, 11, 11, 11/*1000 0x01*/, 11, 34, 11, 34, 11, 34, 11,
        11/*1110 Inverted 8 of command*/, 11, 11, 11, 34, 11, 34, 11, 34/*0111 inverted 1 of command*/, 11 /*stop bit*/};

void loop() {
    const uint8_t NEC_KHZ = 38; // 38kHz carrier frequency for the NEC protocol

#if !(defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__))
    /*
     * Send hand crafted data from RAM
     * The values are NOT multiple of 50, but are taken from the NEC timing definitions
     */
    Serial.println(F("Send NEC 8 bit address 0xFB04, 0x08 with exact timing (16 bit array format)"));

    const uint16_t irSignal[] = { 9000, 4500, 560, 560, 560, 560, 560, 1690, 560, 560, 560, 560, 560, 560, 560, 560, 560, 560,
            560, 1690, 560, 1690, 560, 560, 560, 1690, 560, 1690, 560, 1690, 560, 1690, 560, 1690, 560, 560, 560, 560, 560, 560,
            560, 1690, 560, 560, 560, 560, 560, 560, 560, 560, 560, 1690, 560, 1690, 560, 1690, 560, 560, 560, 1690, 560, 1690, 560,
            1690, 560, 1690, 560 }; // Using exact NEC timing
    IrSender.sendRaw(irSignal, sizeof(irSignal) / sizeof(irSignal[0]), NEC_KHZ); // Note the approach used to automatically calculate the size of the array.

    delay(1000);
#endif

    /*
     * Send byte data direct from FLASH
     * Note the approach used to automatically calculate the size of the array.
     */
    Serial.println(F("Send NEC 16 bit address 0xFB0C, 0x18 with (50 us) tick resolution timing (8 bit array format) "));
    IrSender.sendRaw_P(irSignalP, sizeof(irSignalP) / sizeof(irSignalP[0]), NEC_KHZ);

    delay(1000);

    Serial.println(F("Send NEC 16 bit address 0x0102, 8 bit data 0x34 with generated timing"));
    IrSender.sendNECStandard(0x0102, 0x34, true, 0);

    delay(3000);
}
