#include "ens160.h"
#include <stdio.h>

// External SPI transfer function
static ens160_spi_transfer_t spi_transfer = NULL;

// Private function prototypes
static ENS160_Result ens160_read_register(uint8_t reg_addr, uint8_t *data, uint8_t len);
static ENS160_Result ens160_write_register(uint8_t reg_addr, uint8_t *data, uint8_t len);

// Initialize the driver with SPI transfer function
ENS160_Result ens160_begin(ens160_spi_transfer_t transfer_func) {
    if (transfer_func == NULL) {
        return ENS160_INVALID_PARAM;
    }
    spi_transfer = transfer_func;
    return ENS160_OK;
}

// Initialize the sensor
bool ens160_init(void) {
    uint8_t part_id[2];
    
    // Read PART_ID to verify communication
    if (ens160_read_register(ENS160_PART_ID, part_id, 2) != ENS160_OK) {
        return false;
    }
    
    uint16_t id = (part_id[1] << 8) | part_id[0];
    if (id != 0x0160) {  // ENS160 part ID is 0x0160
        return false;
    }
    
    // Reset the device
    if (ens160_reset() != ENS160_OK) {
        return false;
    }
    
    // Set to IDLE mode
    if (ens160_set_mode(ENS160_OPMODE_IDLE) != ENS160_OK) {
        return false;
    }
    
    return true;
}

// Set operating mode
ENS160_Result ens160_set_mode(uint8_t mode) {
    return ens160_write_register(ENS160_OPMODE, &mode, 1);
}

// Write compensation data (temperature and humidity conversion)
ENS160_Result ens160_write_compensation(uint16_t temp, uint16_t rh) {
    // Temperature: Convert °C to register value (0.005°C per LSB)
    // Humidity: Convert %RH to register value (0.002% per LSB)
    uint8_t temp_data[2] = {temp & 0xFF, (temp >> 8) & 0xFF};
    uint8_t rh_data[2] = {rh & 0xFF, (rh >> 8) & 0xFF};
    
    // Write temperature (LSB first)
    if (ens160_write_register(ENS160_TEMP_IN, temp_data, 2) != ENS160_OK) {
        return ENS160_ERROR;
    }
    
    // Write humidity (LSB first)
    if (ens160_write_register(ENS160_RH_IN, rh_data, 2) != ENS160_OK) {
        return ENS160_ERROR;
    }
    
    return ENS160_OK;
}

// Start standard measurement
ENS160_Result ens160_start_standard_measure(void) {
    return ens160_set_mode(ENS160_OPMODE_STANDARD);
}

// Update sensor data
ENS160_Result ens160_update(void) {
    // Just check if communication is working by reading status
    uint8_t status;
    if (ens160_read_register(ENS160_DEVICE_STATUS, &status, 1) != ENS160_OK) {
        return ENS160_ERROR;
    }
    
    if (status & ENS160_STATUS_STATER) {
        return ENS160_ERROR;  // Device error
    }
    
    return ENS160_OK;
}

// Check if new data is available
bool ens160_has_new_data(void) {
    uint8_t status;
    if (ens160_read_register(ENS160_DEVICE_STATUS, &status, 1) != ENS160_OK) {
        return false;
    }
    return (status & ENS160_STATUS_NEWDAT) != 0;
}

// Get Air Quality Index (AQI bitmask)
uint8_t ens160_get_air_quality_index(void) {
    uint8_t aqi;
    if (ens160_read_register(ENS160_DATA_AQI, &aqi, 1) != ENS160_OK) {
        return 0;
    }
    return aqi & 0x07;  // Only bits 0-2 contain AQI 
}

// Get TVOC value (byte order)
uint16_t ens160_get_tvoc(void) {
    uint8_t tvoc_data[2];
    if (ens160_read_register(ENS160_DATA_TVOC, tvoc_data, 2) != ENS160_OK) {
        return 0;
    }
    // LSB first, then MSB
    return (tvoc_data[1] << 8) | tvoc_data[0];
}

// Get eCO2 value (byte order)
uint16_t ens160_get_eco2(void) {
    uint8_t eco2_data[2];
    if (ens160_read_register(ENS160_DATA_ECO2, eco2_data, 2) != ENS160_OK) {
        return 0;
    }
    // LSB first, then MSB
    return (eco2_data[1] << 8) | eco2_data[0];
}

// Get device status
uint8_t ens160_get_status(void) {
    uint8_t status;
    if (ens160_read_register(ENS160_DEVICE_STATUS, &status, 1) != ENS160_OK) {
        return 0;
    }
    return status;
}

// Reset the device (reset sequence)
ENS160_Result ens160_reset(void) {
    uint8_t reset_cmd = ENS160_OPMODE_RESET;
    if (ens160_write_register(ENS160_OPMODE, &reset_cmd, 1) != ENS160_OK) {
        return ENS160_ERROR;
    }
    
    // Wait for reset to complete (datasheet says 2ms max)
    for (int i = 0; i < 100; i++) {
        uint8_t status;
        if (ens160_read_register(ENS160_DEVICE_STATUS, &status, 1) == ENS160_OK) {
            if (!(status & ENS160_STATUS_STATER)) {
                return ENS160_OK;
            }
        }
        // Small delay
        for (volatile int j = 0; j < 1000; j++);
    }
    
    return ENS160_ERROR;
}

// Read from a register (SPI protocol for ENS160)
static ENS160_Result ens160_read_register(uint8_t reg_addr, uint8_t *data, uint8_t len) {
    if (spi_transfer == NULL) {
        return ENS160_ERROR;
    }
    
    // Prepare transmit buffer: address with read bit set (bit 0 = 1)
    uint8_t tx_buffer[len + 1];
    uint8_t rx_buffer[len + 1];
    
    tx_buffer[0] = (reg_addr << 1) | 0x01;  // Set read bit (correct)
    
    // Remaining bytes are don't care for read operation
    for (int i = 1; i < len + 1; i++) {
        tx_buffer[i] = 0x00;
    }
    
    // Perform SPI transfer
    if (spi_transfer(tx_buffer, rx_buffer, len + 1) != 0) {
        return ENS160_ERROR;
    }
    
    // Copy received data (skip first byte which contains status)
    for (int i = 0; i < len; i++) {
        data[i] = rx_buffer[i + 1];
    }
    
    return ENS160_OK;
}

// Write to a register (SPI protocol for ENS160)
static ENS160_Result ens160_write_register(uint8_t reg_addr, uint8_t *data, uint8_t len) {
    if (spi_transfer == NULL) {
        return ENS160_ERROR;
    }
    
    // Prepare transmit buffer: address with write bit clear (bit 0 = 0)
    uint8_t tx_buffer[len + 1];
    uint8_t rx_buffer[len + 1];
    
    tx_buffer[0] = (reg_addr << 1) & 0xFE;  // Clear read bit (correct)
    
    // Copy data to transmit buffer
    for (int i = 0; i < len; i++) {
        tx_buffer[i + 1] = data[i];
    }
    
    // Perform SPI transfer
    if (spi_transfer(tx_buffer, rx_buffer, len + 1) != 0) {
        return ENS160_ERROR;
    }
    
    return ENS160_OK;
}