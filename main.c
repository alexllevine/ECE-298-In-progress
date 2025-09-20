#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "mxc_device.h"
#include "mxc_delay.h"
#include "spi.h"
#include "gpio.h"
#include "ens160.h"

// MAX78000 FTHR board specific definitions
#define BUILTIN_LED_PORT MXC_GPIO0
#define BUILTIN_LED_PIN MXC_GPIO_PIN_25 // Red LED on FTHR board

// SPI configuration
#define SPI_INSTANCE MXC_SPI0
#define SPI_FREQ 1000000 // 1MHz
#define PIN_CS MXC_GPIO_PIN_6 // Adjust based on wiring

// CO2 Level thresholds (in ppm)
#define CO2_EXCELLENT 600
#define CO2_GOOD      800
#define CO2_MODERATE  1000
#define CO2_BAD       1500  // Above 1500 is terrible

// Enum for CO2 levels
typedef enum {
    EXCELLENT,
    GOOD,
    MODERATE,
    BAD,
    TERRIBLE
} CO2_Level;

// Function prototypes
int init_spi(void);
void init_gpio(void);
CO2_Level getCO2Level(uint16_t eco2);
const char* getCO2LevelText(CO2_Level level);
const char* getCO2Suggestion(CO2_Level level);
void setLEDForCO2Level(CO2_Level level);
int ens160_spi_transfer(uint8_t *tx_data, uint8_t *rx_data, uint32_t len);

// Global variables
static uint32_t lastBlinkTime = 0;
static bool ledState = false;

// Initializing SPI for MAX78000
int init_spi(void) {
    int error;

    // Configure SPI
    mxc_spi_cfg_t spi_cfg = {
        .mode = SPI_MODE_0, // SPI mode 0
        .ssel = 0,          // Software-controlled Chip Select
        .freq = SPI_FREQ,   // Communication speed (1MHz)
        .bits = 8,          // 8 bits per transfer
        .clk_pol = 0,       // Clock polarity: idle low
        .clk_pha = 0        // Clock phase: sample on first edge
    };

    if ((error = MXC_SPI_Init(SPI_INSTANCE, &spi_cfg, true)) != E_NO_ERROR) {
        return error;
    }

    return E_NO_ERROR;
}

// Initialize GPIO for CS pin
void init_gpio(void) {
    // Configure CS pin as output
    mxc_gpio_cfg_t cs_pin = {MXC_GPIO0, PIN_CS, MXC_GPIO_FUNC_OUT, MXC_GPIO_PAD_NONE, MXC_GPIO_VSSEL_VDDIO};
    MXC_GPIO_Config(&cs_pin);
    MXC_GPIO_OutSet(MXC_GPIO0, PIN_CS); // Set CS high initially

    // Configure LED pin
    mxc_gpio_cfg_t led_pin = {MXC_GPIO0, BUILTIN_LED_PIN, MXC_GPIO_FUNC_OUT, MXC_GPIO_PAD_NONE, MXC_GPIO_VSSEL_VDDIO};
    MXC_GPIO_Config(&led_pin);
    MXC_GPIO_OutClr(MXC_GPIO0, BUILTIN_LED_PIN); // Turn off LED initially
}

// Determine CO2 level based on concentration
CO2_Level getCO2Level(uint16_t eco2) {
    if (eco2 <= CO2_EXCELLENT) return EXCELLENT;
    if (eco2 <= CO2_GOOD) return GOOD;
    if (eco2 <= CO2_MODERATE) return MODERATE;
    if (eco2 <= CO2_BAD) return BAD;
    return TERRIBLE;
}

// Get text description for CO2 level
const char* getCO2LevelText(CO2_Level level) {
    switch(level) {
        case EXCELLENT: return "Excellent";
        case GOOD: return "Good";
        case MODERATE: return "Moderate";
        case BAD: return "Bad";
        case TERRIBLE: return "Terrible";
        default: return "Unknown";
    }
}

// Get suggestion based on CO2 level
const char* getCO2Suggestion(CO2_Level level) {
    switch(level) {
        case EXCELLENT: return "No suggestion";
        case GOOD: return "Keep normal";
        case MODERATE: return "It is OK to ventilate";
        case BAD: return "Indoor air is polluted/Ventilation is recommended";
        case TERRIBLE: return "Indoor air pollution is serious/Ventilation is required";
        default: return "";
    }
}

// Control built-in LED based on CO2 level
void setLEDForCO2Level(CO2_Level level) {
    static uint32_t lastBlinkTime = 0;
    static bool ledState = false;
    uint32_t currentTime = MXC_DelayGetMS();

    switch(level) {
        case EXCELLENT:
        case GOOD:
            // Good air quality - turn LED off
            MXC_GPIO_OutClr(MXC_GPIO0, BUILTIN_LED_PIN);
            break;

        case MODERATE:
            // Moderate air quality - slow blink (1 second interval)
            if (currentTime - lastBlinkTime >= 1000) {
                lastBlinkTime = currentTime;
                ledState = !ledState;
                if (ledState) {
                    MXC_GPIO_OutSet(MXC_GPIO0, BUILTIN_LED_PIN);
                } else {
                    MXC_GPIO_OutClr(MXC_GPIO0, BUILTIN_LED_PIN);
                }
            }
            break;

        case BAD:
        case TERRIBLE:
            // Bad air quality - fast blink (250ms interval)
            if (currentTime - lastBlinkTime >= 250) {
                lastBlinkTime = currentTime;
                ledState = !ledState;
                if (ledState) {
                    MXC_GPIO_OutSet(MXC_GPIO0, BUILTIN_LED_PIN);
                } else {
                    MXC_GPIO_OutClr(MXC_GPIO0, BUILTIN_LED_PIN);
                }            
            }
            break;    
    }
}

// SPI transfer function for ENS160
int ens160_spi_transfer(uint8_t *tx_data, uint8_t *rx_data, uint32_t len) {
    mxc_spi_req_t req = {
        .spi = SPI_INSTANCE,    // Use SPI0 peripheral
        .ssIdx = 0,             // Slave select index
        .ssDeassert = 1,        // Automatically deassert CS after transfer
        .txData = tx_data,      // Data to transmit to sensor
        .rxData = rx_data,      // Buffer for received data from sensor
        .txLen = len,           // Number of bytes to transmit
        .rxLen = len,           // Number of bytes to receive
        .txCnt = 0,             // Reset transmit counter
        .rxCnt = 0,             // Reset receive counter
        .completeCB = NULL      // No callback function (blocking operation)
    };

    // Assert CS
    MXC_GPIO_OutClr(MXC_GPIO0, PIN_CS);

    int error = MXC_SPI_MasterTransaction(&req);

    // Deassert CS
    MXC_GPIO_OutSet(MXC_GPIO0, PIN_CS);

    return error;
}

int main(void) {
    printf("Initializing ENS160 sensor for MAX78000 FTHR...\n");

    // Initialize system components
    MXC_Delay(MXC_DELAY_MSEC(1000)); // Allow time for startup

    init_gpio();
    if (init_spi() != E_NO_ERROR) {
        printf("SPI initialization failed!\n");
        return -1;
    }

    printf("Initializing ENS160 sensor...\n");

    // Initialize ENS160 sensor
    ens160_begin(ens160_spi_transfer);

    while (!ens160_init()) {
        printf(".");
        MXC_Delay(MXC_DELAY_MSEC(1000));
    }

    printf("\nSuccess\n");

    // Set ambient conditions for compensation
    float tempC = 25.0;
    float rh = 50.0;

    // Write compensation data
    ens160_write_compensation((tempC + 273.15) * 64, rh * 512);
    printf("Set compensation: %.1f°C, %.1f%%RH\n", tempC, rh);

    // Start standard measurement
    ens160_start_standard_measure();
    printf("ENS160 in STANDARD mode.\n");

    printf("Waiting for sensor to stabilize....\n");
    
    // Blink LED while stabilizing (repeats 5 times)
    for (int i = 0; i < 5; i++) {        
        MXC_GPIO_OutSet(MXC_GPIO0, BUILTIN_LED_PIN); // Turn LED on
        MXC_Delay(MXC_DELAY_MSEC(200));   // Wait 200 ms
        MXC_GPIO_OutClr(MXC_GPIO0, BUILTIN_LED_PIN); // Turn LED off
        MXC_Delay(MXC_DELAY_MSEC(200));   // Wait 200 ms
    }

    // Structure of infinite loop 
    //  Read sensors --> Process data --> Output results --> wait
    while (1) {
        // Wait for sensor data and update
        if (ens160_update() == ENS160_OK) {
            if (ens160_has_new_data()) {
                // Get sensor values
                uint8_t aqi = ens160_get_air_quality_index();
                uint16_t tvoc = ens160_get_tvoc();
                uint16_t eco2 = ens160_get_eco2();
                
                // Determine CO2 level
                CO2_Level co2level = getCO2Level(eco2);

                // Print results
                printf("=== ENS160 Sensor Data ===\n");
                printf("Air Quality Index: %d\n", aqi);
                printf("TVOC: %d ppb\n", tvoc);
                printf("eCO2: %d ppm\n", eco2);
                printf("CO2 Level: %s (%d ppm)\n", getCO2LevelText(co2level), eco2);
                printf("Suggestion: %s\n\n", getCO2Suggestion(co2level));

                // Update LED based on CO2 level
                setLEDForCO2Level(co2level);
            } else {
                printf("No new data available.\n");

                // Briefly flash LED
                MXC_GPIO_OutSet(MXC_GPIO0, BUILTIN_LED_PIN);
                MXC_Delay(MXC_DELAY_MSEC(50));
                MXC_GPIO_OutClr(MXC_GPIO0, BUILTIN_LED_PIN);
            }
        } else {
            printf("Error updating data.\n");

            // Flash LED to indicate error
            MXC_GPIO_OutSet(MXC_GPIO0, BUILTIN_LED_PIN);
            MXC_Delay(MXC_DELAY_MSEC(100));
            MXC_GPIO_OutClr(MXC_GPIO0, BUILTIN_LED_PIN);              
        }

        // Wait before next reading
        MXC_Delay(MXC_DELAY_MSEC(2000));
    }

    return 0;
}