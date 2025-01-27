/*
 * @file irPronto.cpp
 * @brief In this file, the functions IRrecv::dumpPronto and IRsend::sendPronto are defined.
 *
 * See http://www.harctoolbox.org/Glossary.html#ProntoSemantics
 * Pronto database http://www.remotecentral.com/search.htm
 *
 *  This file is part of Arduino-IRremote https://github.com/z3t0/Arduino-IRremote.
 *
 ************************************************************************************
 * MIT License
 *
 * Copyright (c) 2020 Bengt Martensson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ************************************************************************************
 */
//#define DEBUG // Activate this for lots of lovely debug output.
#include "IRremote.h"

// DO NOT EXPORT from this file
static const uint16_t MICROSECONDS_T_MAX = 0xFFFFU;
static const uint16_t learnedToken = 0x0000U;
static const uint16_t learnedNonModulatedToken = 0x0100U;
static const unsigned int bitsInHexadecimal = 4U;
static const unsigned int digitsInProntoNumber = 4U;
static const unsigned int numbersInPreamble = 4U;
static const unsigned int hexMask = 0xFU;
static const uint32_t referenceFrequency = 4145146UL;
static const uint16_t fallbackFrequency = 64767U; // To use with frequency = 0;
static const uint32_t microsecondsInSeconds = 1000000UL;
static const uint16_t PRONTO_DEFAULT_GAP = 45000;

static unsigned int toFrequencyKHz(uint16_t code) {
    return ((referenceFrequency / code) + 500) / 1000;
}

/*
 * Parse the string given as Pronto Hex, and send it a number of times given as argument.
 */
void IRsend::sendPronto(const uint16_t *data, unsigned int size, uint8_t numberOfRepeats) {
    unsigned int timebase = (microsecondsInSeconds * data[1] + referenceFrequency / 2) / referenceFrequency;
    unsigned int khz;
    switch (data[0]) {
    case learnedToken: // normal, "learned"
        khz = toFrequencyKHz(data[1]);
        break;
    case learnedNonModulatedToken: // non-demodulated, "learned"
        khz = 0U;
        break;
    default:
        return; // There are other types, but they are not handled yet.
    }
    unsigned int intros = 2 * data[2];
    unsigned int repeats = 2 * data[3];
    DBG_PRINT(F("intros="));
    DBG_PRINT(intros);
    DBG_PRINT(F(" repeats="));
    DBG_PRINTLN(repeats);
    if (numbersInPreamble + intros + repeats != size) { // inconsistent sizes
        return;
    }

    /*
     * Generate a new microseconds timing array for sendRaw.
     * If recorded by IRremote, intro contains the whole IR data and repeat is empty
     */
    uint16_t durations[intros + repeats];
    for (unsigned int i = 0; i < intros + repeats; i++) {
        uint32_t duration = ((uint32_t) data[i + numbersInPreamble]) * timebase;
        durations[i] = (unsigned int) ((duration <= MICROSECONDS_T_MAX) ? duration : MICROSECONDS_T_MAX);
    }

    /*
     * Send the intro. intros is even.
     * Do not send the trailing space here, send it if repeats are requested
     */
    if (intros >= 2) {
        sendRaw(durations, intros - 1, khz);
    }

    if (repeats == 0 || numberOfRepeats == 0) {
        // only send intro once
        return;
    }

    /*
     * Now send the trailing space/gap of the intro and all the repeats
     */
    delay(durations[intros - 1] / 1000U); // equivalent to space(durations[intros - 1]); but allow bigger values for the gap
    for (unsigned int i = 0; i < numberOfRepeats; i++) {
        sendRaw(durations + intros, repeats - 1, khz);
        if ((i + 1) < numberOfRepeats) { // skip last trailing space/gap, see above
            delay(durations[intros + repeats - 1] / 1000U);
        }
    }
}

void IRsend::sendPronto(const char *str, uint8_t numberOfRepeats) {
    size_t len = strlen(str) / (digitsInProntoNumber + 1) + 1;
    uint16_t data[len];
    const char *p = str;
    char *endptr[1];
    for (unsigned int i = 0; i < len; i++) {
        long x = strtol(p, endptr, 16);
        if (x == 0 && i >= numbersInPreamble) {
            // Alignment error?, bail immediately (often right result).
            len = i;
            break;
        }
        data[i] = static_cast<uint16_t>(x); // If input is conforming, there can be no overflow!
        p = *endptr;
    }
    sendPronto(data, len, numberOfRepeats);
}

#if defined(__AVR__)
void IRsend::sendPronto_PF(uint_farptr_t str, uint8_t numberOfRepeats) {
    size_t len = strlen_PF(str);
    char work[len + 1];
    strncpy_PF(work, str, len);
    sendPronto(work, numberOfRepeats);
}
void IRsend::sendPronto_P(const char* str, uint8_t numberOfRepeats) {
    size_t len = strlen_P(str);
    char work[len + 1];
    strncpy_P(work, str, len);
    sendPronto(work, numberOfRepeats);
}
#endif

void IRsend::sendPronto(const __FlashStringHelper *str, uint8_t numberOfRepeats) {
    size_t len = strlen_P(reinterpret_cast<const char*>(str));
    char work[len + 1];
    strncpy_P(work, reinterpret_cast<const char*>(str), len);
    return sendPronto(work, numberOfRepeats);
}

static uint16_t effectiveFrequency(uint16_t frequency) {
    return frequency > 0 ? frequency : fallbackFrequency;
}

static uint16_t toTimebase(uint16_t frequency) {
    return microsecondsInSeconds / effectiveFrequency(frequency);
}

static uint16_t toFrequencyCode(uint16_t frequency) {
    return referenceFrequency / effectiveFrequency(frequency);
}

static char hexDigit(unsigned int x) {
    return (char) (x <= 9 ? ('0' + x) : ('A' + (x - 10)));
}

static void dumpDigit(Print *aSerial, unsigned int number) {
    aSerial->print(hexDigit(number));
}

static void dumpNumber(Print *aSerial, uint16_t number) {
    for (unsigned int i = 0; i < digitsInProntoNumber; i++) {
        unsigned int shifts = bitsInHexadecimal * (digitsInProntoNumber - 1 - i);
        dumpDigit(aSerial, (number >> shifts) & hexMask);
    }
    aSerial->print(' ');
}

static void dumpDuration(Print *aSerial, uint32_t duration, uint16_t timebase) {
    dumpNumber(aSerial, (duration + timebase / 2) / timebase);
}

/*
 * Compensate received values by MARK_EXCESS_MICROS, like it is done for decoding!
 */
static void dumpSequence(Print *aSerial, const volatile uint16_t *data, size_t length, uint16_t timebase) {
    for (uint8_t i = 0; i < length; i++) {
        uint32_t tDuration = data[i] * MICROS_PER_TICK;
        if (i & 1) {
            // Mark
            tDuration -= MARK_EXCESS_MICROS;
        } else {
            tDuration += MARK_EXCESS_MICROS;
        }
        dumpDuration(aSerial, tDuration, timebase);
    }

    // append a gap
    dumpDuration(aSerial, PRONTO_DEFAULT_GAP, timebase);
}

/*
 * Using Print instead of Stream saves 1020 bytes program memory
 * Changed from & to * parameter type to be more transparent and consistent with other code of IRremote
 */
void IRrecv::dumpPronto(Print *aSerial, unsigned int frequency) {
    dumpNumber(aSerial, frequency > 0 ? learnedToken : learnedNonModulatedToken);
    dumpNumber(aSerial, toFrequencyCode(frequency));
    dumpNumber(aSerial, (results.rawlen + 1) / 2);
    dumpNumber(aSerial, 0);
    unsigned int timebase = toTimebase(frequency);
    dumpSequence(aSerial, &results.rawbuf[1], results.rawlen - 1, timebase); // skip leading space
}

//+=============================================================================
// Dump out the raw data as Pronto Hex.
// I know Stream * is locally inconsistent, but all global print functions use it
//
void IRrecv::printIRResultAsPronto(Print *aSerial, unsigned int frequency) {
    aSerial->println("Pronto Hex as string");
    aSerial->print("char ProntoData[] = \"");
    dumpPronto(aSerial, frequency);
    aSerial->println("\"");
}

/*
 * Functions for dumping Pronto to a String. This is not very time and space efficient
 * and can lead to resource problems especially on small processors like AVR's
 */

static bool dumpDigit(String *aString, unsigned int number) {
    return aString->concat(hexDigit(number));
}

static size_t dumpNumber(String *aString, uint16_t number) {

    size_t size = 0;

    for (unsigned int i = 0; i < digitsInProntoNumber; i++) {
        unsigned int shifts = bitsInHexadecimal * (digitsInProntoNumber - 1 - i);
        size += dumpDigit(aString, (number >> shifts) & hexMask);
    }
    size += aString->concat(' ');

    return size;
}

/*
 * Compensate received values by MARK_EXCESS_MICROS, like it is done for decoding!
 */
static size_t dumpDuration(String *aString, uint32_t duration, uint16_t timebase) {
    return dumpNumber(aString, (duration + timebase / 2) / timebase);
}

static size_t dumpSequence(String *aString, const volatile uint16_t *data, size_t length, uint16_t timebase) {

    size_t size = 0;

    for (uint8_t i = 0; i < length; i++) {
        uint32_t tDuration = data[i] * MICROS_PER_TICK;
        if (i & 1) {
            // Mark
            tDuration -= MARK_EXCESS_MICROS;
        } else {
            tDuration += MARK_EXCESS_MICROS;
        }
        size += dumpDuration(aString, tDuration, timebase);
    }

    // append minimum gap
    size += dumpDuration(aString, PRONTO_DEFAULT_GAP, timebase);

    return size;
}

/*
 * Writes Pronto HEX to a String object.
 * Returns the amount of characters added to the string.(360 characters for a NEC code!)
 */
size_t IRrecv::dumpPronto(String *aString, unsigned int frequency) {

    size_t size = 0;
    unsigned int timebase = toTimebase(frequency);

    size += dumpNumber(aString, frequency > 0 ? learnedToken : learnedNonModulatedToken);
    size += dumpNumber(aString, toFrequencyCode(frequency));
    size += dumpNumber(aString, (results.rawlen + 1) / 2);
    size += dumpNumber(aString, 0);
    size += dumpSequence(aString, &results.rawbuf[1], results.rawlen - 1, timebase); // skip leading space

    return size;
}
