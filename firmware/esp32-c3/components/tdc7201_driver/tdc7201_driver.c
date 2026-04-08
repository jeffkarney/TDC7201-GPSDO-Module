#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "i2c.h"  // I2C library for ESP32

#define TDC7201_I2C_ADDRESS 0x20  // Example I2C address

typedef struct {
    uint8_t i2c_address;
    uint32_t clock_speed;
    bool measurement_mode;  // true for continuous, false for single
} tdc7201_config_t;

static tdc7201_config_t tdc7201_config;

void tdc7201_init(uint8_t i2c_address, uint32_t clock_speed, bool measurement_mode) {
    tdc7201_config.i2c_address = i2c_address;
    tdc7201_config.clock_speed = clock_speed;
    tdc7201_config.measurement_mode = measurement_mode;

    // Configure device via I2C
    if (i2c_write(tdc7201_config.i2c_address, /* config registers */) != 0) {
        printf("I2C Communication Error in tdc7201_init\n");
    }
}

void tdc7201_start_measurement() {
    // Send START/STOP conversion command via I2C
    if (i2c_write(tdc7201_config.i2c_address, /* start command */) != 0) {
        printf("I2C Communication Error in tdc7201_start_measurement\n");
    }
}

uint32_t tdc7201_read_measurement() {
    uint32_t result = 0;
    if (i2c_read(tdc7201_config.i2c_address, /* measurement register */, &result, sizeof(result)) != 0) {
        printf("I2C Communication Error in tdc7201_read_measurement\n");
    }
    return result;  // Result should be in picoseconds
}

bool tdc7201_get_status() {
    uint8_t status;
    if (i2c_read(tdc7201_config.i2c_address, /* status register */, &status, sizeof(status)) != 0) {
        printf("I2C Communication Error in tdc7201_get_status\n");
        return false;
    }
    return status & 0x01;  // Example bit for ready status
}

void tdc7201_calibrate_delay_line(uint8_t value) {
    // Access calibration register
    if (i2c_write(tdc7201_config.i2c_address, /* calibration register */, &value, sizeof(value)) != 0) {
        printf("I2C Communication Error in tdc7201_calibrate_delay_line\n");
    }
}