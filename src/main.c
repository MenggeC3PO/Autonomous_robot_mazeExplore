 /*
 * MAIN Generated Driver File
 * 
 * @file main.c
 * 
 * @defgroup main MAIN
 * 
 * @brief This is the generated driver implementation file for the MAIN driver.
 *
 * @version MAIN Driver Version 1.0.2
 *
 * @version Package Version: 3.1.2
*/

/* 
 * File:   barcode_fsm.h
 * Author: zhang
 *
 * Created on May 6, 2025, 10:30 AM
 */

// Interrupts-controlled: SPI2/ADC/NRF 
// TMR0-NRF
// TMR1-ADC
// SPI2 Salve wait for ESP Host 

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "mcc_generated_files/system/system.h"
#include "mirf.h"
#include "barcode_fsm.h"
// ==================== Global definition ====================
void TMR0_InterruptHandler(void);
volatile bool spiDataAvailable = false;
uint8_t spi_buffer[32];
char barcode[9];// two shared buffer   
static uint8_t search_index = 0;

// ==================== Nrf config ====================
#define CONFIG_RADIO_CHANNEL 110
#define CONFIG_ADVANCED 1
#define CONFIG_RF_RATIO_1M 1


// ==================== Nrf symbel ====================
typedef struct __attribute__((__packed__)) {
    uint8_t group_id;
    uint8_t part_id;
    uint8_t total;
    uint8_t type;   // 0 = search, 1 = solve
    uint8_t other;
    uint8_t data[27];
} NRFsymbel;
// ==================== Maze-frames ==================== 
typedef struct {
    uint8_t x, y, dir;
    bool wall_left, wall_right, wall_up, wall_down;
} SearchFrame;

typedef struct {
    uint8_t x, y, dir;
    bool has_barcode;
    uint8_t barcode;
} SolveFrame;

uint8_t encode_search_byte(SearchFrame s) {
    uint8_t val = 0;
    if (s.wall_left)  val |= 0x01;
    if (s.wall_right) val |= 0x02;
    if (s.wall_up)    val |= 0x04;
    if (s.wall_down)  val |= 0x08;
    val |= (s.dir & 0x07) << 4;
    return val;
}

void encode_solve_bytes(SolveFrame s, uint8_t *buf) {
    buf[0] = s.x;
    buf[1] = s.y;
    buf[2] = (s.dir & 0x03) | (s.has_barcode ? 0x80 : 0);
    buf[3] = s.has_barcode ? s.barcode : 0xFF;
    memset(&buf[4], 0, 23);
}
// ==================== simulated data ====================
const SearchFrame mazeSearchData[] = {
    {0,0,0,1,0,1,0}, {1,0,1,0,0,1,1}, {2,0,1,0,0,1,0}, {3,0,1,0,0,1,1}, {4,0,1,0,1,1,0},
    {4,1,2,1,1,0,0}, {4,2,2,1,1,0,0}, {4,3,2,0,1,0,1}, {3,3,3,0,0,1,1}, {2,3,3,0,0,1,1},
    {1,3,3,0,0,1,1}, {0,3,3,1,0,0,1}, {0,2,0,1,0,0,0}, {0,1,0,1,1,0,0}, {0,2,2,1,0,0,0},
    {1,2,1,0,0,1,1}, {2,2,1,0,1,0,1}, {2,1,0,1,1,0,0}, {2,0,0,0,0,1,0}, {1,0,3,0,0,1,1},
    {0,0,3,1,0,1,0}
};
// ==================== NRF Setting ====================
#if CONFIG_ADVANCED
void AdvancedSettings(NRF24_t * dev)
{
#if CONFIG_RF_RATIO_1M
    Nrf24_SetSpeedDataRates(dev, 0);
#endif
}
#endif

// ====================  Sender_step ====================
static NRF24_t nrf;
static bool nrf_inited = false;
static uint8_t group_id = 1;
static uint8_t index = 0;
static uint8_t mode = 0; // 0 = search, 1 = solve
/*
    Main application
*/

void main(void)
{
    //CLRWDT(); 
    SYSTEM_Initialize();
    UART1_Enable();
    printf("[BOOT] System initialized\r\n");
    //==================== Open interrupts ====================
    INTERRUPT_GlobalInterruptEnable(); 
    printf("[INTERRUPT] Open all interrupts\r\n");
    // ====================  ADC-TMR1-auto trigger ====================
    TMR1_Start();
    ADC_ConversionDoneCallbackRegister(My_ADC_Callback);
    ADC_ConversionDoneInterruptEnable();
    printf("[ADC] TMR1 + Callback ready\r\n");

    
    // ====================  Nrf-Setup ====================
    Nrf24_init(&nrf);
    Nrf24_config(&nrf, 110, 32);
    Nrf24_disableAA(&nrf);
    Nrf24_SetCRCLength(&nrf, RF24_CRC_DISABLED);
    Nrf24_setTADDR(&nrf, (uint8_t *)"ABCDE");
    AdvancedSettings(&nrf);
    printf("[NRF] NRF24L01 setup complete\r\n");
    
    // ====================  TMR0-Nrf-Sender ====================
    TMR0_PeriodMatchCallbackRegister(TMR0_InterruptHandler);
    TMR0_Start();
    printf("[TMR0] Periodic sender enabled\r\n");


    while (1)
    {
       
        //printf("[LOOP] Still alive\r\n");

    }

}
// ====================  Nrf sending Interrupt ====================
void TMR0_InterruptHandler(void)
{
    printf("[TMR0]Callback\r\n");
    static NRFsymbel packet;

    packet.group_id = group_id++;
    packet.part_id = 0;
    packet.total = 1;
    packet.other = 0;

    // Search mode (frame_id = 1~21, simulated from maze frames)
    if (search_index < sizeof(mazeSearchData) / sizeof(SearchFrame))
    {
        packet.group_id = search_index + 1;
        packet.type = 0;
        packet.part_id = 0;
        packet.total = 1;
        packet.other = 0;

        // Build search frame 
        packet.data[0] = encode_search_byte(mazeSearchData[search_index]);
        packet.data[1] = mazeSearchData[search_index].x;
        packet.data[2] = mazeSearchData[search_index].y;
        memset(&packet.data[3], 0, 24);

        Nrf24_send(&nrf, (uint8_t*)&packet);
        if (Nrf24_isSend(&nrf, 1000))
            printf("[TMR0] Search frame %u sent (x=%u, y=%u)\r\n",
                   packet.group_id, packet.data[1], packet.data[2]);
        else
            printf("[TMR0] Search frame %u failed\r\n", packet.group_id);

        search_index++;  
        //
        if (barcodeAvailable)
        {
            SolveFrame sf;
            sf.x = mazeSearchData[search_index - 1].x;  
            sf.y = mazeSearchData[search_index - 1].y;
            sf.dir = (packet.data[0] >> 4) & 0x07;      // keep (x,y)and dir from the last search frame
            sf.has_barcode = true;
            sf.barcode = (uint8_t)strtol(barcode, NULL, 2);

            printf("[TMR0] Barcode ready: %s\r\n", barcode);  // DEBUG for BARCODE GET
            encode_solve_bytes(sf, packet.data);
            packet.type = 1;
            packet.group_id = group_id++;

            printf("[TMR0] Sending group_id: %u, type: %u, barcode: %s (dec: %u)\r\n",//DEBUG fro BARCODE-SOLVE FRAME
            packet.group_id, packet.type, barcode, sf.barcode);

            Nrf24_send(&nrf, (uint8_t*)&packet);
    
            if(Nrf24_isSend(&nrf, 1000))
                printf("Send success\r\n");
            else
                printf("Send failed\r\n");
            barcodeAvailable = false;  // reset the barcode flag
    }

   }
}
