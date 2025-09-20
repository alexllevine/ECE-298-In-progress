#ifndef ENS160_H
#define ENS160_H

#include <stdint.h>
#include <stdbool.h>

// ENS160 Register Addresses (Verify against datasheet)
#define ENS160_PART_ID          0x00  
#define ENS160_OPMODE           0x10  
#define ENS160_CONFIG           0x11  
#define ENS160_COMMAND          0x12  
#define ENS160_TEMP_IN          0x13  
#define ENS160_RH_IN            0x15  
#define ENS160_DEVICE_STATUS    0x20  
#define ENS160_DATA_AQI         0x21  
#define ENS160_DATA_TVOC        0x22  
#define ENS160_DATA_ECO2        0x24  
#define ENS160_DATA_T           0x30  
#define ENS160_DATA_RH          0x32  
#define ENS160_DATA_MISR        0x38  
#define ENS160_GPR_WRITE        0x40  
#define ENS160_GPR_READ         0x48  

// Operating Modes (Verified)
#define ENS160_OPMODE_DEEPSLEEP 0x00  
#define ENS160_OPMODE_IDLE      0x01  
#define ENS160_OPMODE_STANDARD  0x02  
#define ENS160_OPMODE_RESET     0xF0  

// Commands (Verified)
#define ENS160_COMMAND_NOP         0x00 
#define ENS160_COMMAND_GET_APPVER  0x0E  
#define ENS160_COMMAND_CLRGPR      0xCC  

// Status flags (Verified)
#define ENS160_STATUS_STATER       0x40  // Error bit
#define ENS160_STATUS_NEWDAT       0x02  // New data available 
#define ENS160_STATUS_NEWGPR       0x01  // New GPR data available

// Function results
typedef enum {
    ENS160_OK,
    ENS160_ERROR,
    ENS160_NOT_READY,
    ENS160_INVALID_PARAM
} ENS160_Result;

// Function pointer type for SPI transfer
typedef int (*ens160_spi_transfer_t)(uint8_t *tx_data, uint8_t *rx_data, uint32_t len);

// Function prototypes
ENS160_Result ens160_begin(ens160_spi_transfer_t spi_transfer);
bool ens160_init(void);
ENS160_Result ens160_set_mode(uint8_t mode);
ENS160_Result ens160_write_compensation(uint16_t temp, uint16_t rh);
ENS160_Result ens160_start_standard_measure(void);
ENS160_Result ens160_update(void);
bool ens160_has_new_data(void);
uint8_t ens160_get_air_quality_index(void);
uint16_t ens160_get_tvoc(void);
uint16_t ens160_get_eco2(void);
uint8_t ens160_get_status(void);
ENS160_Result ens160_reset(void);

#endif // ENS160_H