/* 
 * File:   barcode_fsm.h
 * Author: zhang
 *
 * Created on May 5, 2025, 10:30 PM
 */
// Done the Darcode-detecting by: 
// Auto-trigger by TMR1, every 10 ms
// ADC-Conversion-Done interrupt callback the FSM(States: Barcode-Idle and Barcode Scaning )
// Valid count window for Barcode scanning; Fixed valid adc values to make sure enough values 
// BitStream clean to delete folor value inside valid values; 
// Generate final 8-bits Barcode by radio and group numbers

#include "barcode_fsm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xc.h>
#include "mcc_generated_files/adc/adc.h"

#define BLACK 4000
#define WHITE 1400
#define MAX_BITS 256
#define ENTRY_COUNT 5
#define MIN_GROUP_LEN 1

typedef enum {
    FSM_IDLE, FSM_SCANNING
} FSM_State;

static volatile FSM_State fsm_state = FSM_IDLE;
static volatile adc_result_t adc_sample = 0;
static volatile int bit = 2;
static volatile int validCount = 0;

static uint16_t sample_timer = 0;
static uint8_t bitStream[MAX_BITS];
static uint8_t bitStreamLen = 0;

char barcode[9] = {0};
volatile uint8_t barcodeAvailable = 0;

static int adc_to_bit(adc_result_t val)
{
    if (val > BLACK - 70) return 1;
    else if (val < WHITE + 70) return 0;
    else return 2;
}

static void cleanBitStream(uint8_t *stream, uint8_t *len)
{
    uint8_t cleaned[MAX_BITS] = {0};
    uint8_t prev = stream[0];
    int count = 1, outIdx = 0;

    for (int i = 1; i < *len; i++) {
        if (stream[i] == prev) {
            count++;
        } else {
            if (count >= MIN_GROUP_LEN)
                for (int j = 0; j < count && outIdx < MAX_BITS; j++)
                    cleaned[outIdx++] = prev;
            prev = stream[i];
            count = 1;
        }
    }
    if (count >= MIN_GROUP_LEN)
        for (int j = 0; j < count && outIdx < MAX_BITS; j++)
            cleaned[outIdx++] = prev;

    memcpy(stream, cleaned, outIdx);
    *len = outIdx;
}

static void generateBarcodeFromStream(uint8_t *bits, uint8_t len)
{
    if (len < 8) return;

    int groupLens[16] = {0};
    int bitType[16] = {0};
    int groupCount = 0, sum = 0;

    int pre = bits[0], count = 1;
    for (int i = 1; i < len; i++) {
        if (bits[i] == pre) {
            count++;
        } else {
            bitType[groupCount] = pre;
            groupLens[groupCount++] = count;
            pre = bits[i];
            count = 1;
        }
    }
    if (count > 0 && groupCount < 16) {
        bitType[groupCount] = pre;
        groupLens[groupCount++] = count;
    }

    for (int i = 0; i < groupCount; i++) sum += groupLens[i];

    int assigned[16] = {0}, total = 0;
    for (int i = 0; i < groupCount; i++) {
        assigned[i] = (int)((groupLens[i] * 8.0 / sum) + 0.5);
        if (assigned[i] == 0) assigned[i] = 1;
        total += assigned[i];
    }

    while (total < 8)
        for (int i = 0; i < groupCount && total < 8; i++, total++) assigned[i]++;
    while (total > 8)
        for (int i = 0; i < groupCount && total > 8; i++)
            if (assigned[i] > 1) { assigned[i]--; total--; }

    int idx = 0;
    for (int i = 0; i < groupCount && idx < 8; i++)
        for (int j = 0; j < assigned[i] && idx < 8; j++)
            barcode[idx++] = (bitType[i] == 1) ? '1' : '0';
    barcode[8] = '\0';
    barcodeAvailable = 1;

    // ====== DEBUG PRINTS ======
    printf("[FSM] Barcode = %s\r\n", barcode);
    printf("[FSM] GroupCount = %d\r\n", groupCount);
    for (int i = 0; i < groupCount; i++) {
        printf("  Group %d: bitType = %d (%s), length = %d, assigned = %d\r\n",
               i,
               bitType[i],
               bitType[i] == 1 ? "BLACK" : "WHITE",
               groupLens[i],
               assigned[i]);
    }
}

void My_ADC_Callback(void)
{
    static int printCounter = 0;
    printCounter++;
    if (printCounter >= 10) {
    printf("[ADC]callback\r\n");
    printCounter =0;
    }
    adc_sample = ADC_ConversionResultGet();
    bit = adc_to_bit(adc_sample);

    static uint8_t entryBuffer[ENTRY_COUNT] = {0};
    static int entryIndex = 0;
    /* DEBUG ADC-VALUES for default values 
    if (bit == 0)
        printf("[ADC] Sample = %u (WHITE)\r\n", adc_sample);
    else if (bit == 1)
        printf("[ADC] Sample = %u (BLACK)\r\n", adc_sample);
    else
        printf("[ADC] Sample = %u (FOLOR)\r\n", adc_sample);
     */
    switch (fsm_state) {
        case FSM_IDLE:
            if (bit == 0 || bit == 1) {
                entryBuffer[entryIndex++ % ENTRY_COUNT] = bit;
                validCount++;
                if (validCount >= ENTRY_COUNT) {
                    printf("[FSM] ENTER BARCODE capture\r\n");
                    fsm_state = FSM_SCANNING;
                    bitStreamLen = 0;
                    sample_timer = 0;
                    validCount = 0;
                    for (int i = 0; i < ENTRY_COUNT && bitStreamLen < MAX_BITS; i++)
                        bitStream[bitStreamLen++] = entryBuffer[i];
                }
            } else {
                validCount = 0;
                entryIndex = 0;
            }
            break;

        case FSM_SCANNING:
            if (bit == 0 || bit == 1) {
                if (bitStreamLen < MAX_BITS)
                    bitStream[bitStreamLen++] = bit;
            }
            sample_timer++;
            if (sample_timer >= MAX_BITS) {
                printf("[FSM] EXIT BARCODE: len = %d\r\n", bitStreamLen);
                cleanBitStream(bitStream, &bitStreamLen);
                generateBarcodeFromStream(bitStream, bitStreamLen);
                fsm_state = FSM_IDLE;
            }
            break;
    }
}