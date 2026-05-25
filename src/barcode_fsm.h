/* 
 * File:   barcode_fsm.h
 * Author: zhang
 *
 * Created on May 5, 2025, 10:30 PM
 */

// barcode_fsm.h - Header for barcode FSM logic
#ifndef BARCODE_FSM_H
#define BARCODE_FSM_H

#include <stdint.h>
#include "mcc_generated_files/adc/adc.h"

#ifdef __cplusplus
extern "C" {
#endif

// ??????????????
extern char barcode[9];              // ??????8?????????
extern volatile uint8_t barcodeAvailable;  // ??????????????

// ????? ADC ????
void My_ADC_Callback(void);

#ifdef __cplusplus
}
#endif

#endif // BARCODE_FSM_H





