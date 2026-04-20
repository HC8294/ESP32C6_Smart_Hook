/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define DEFAULT_POWER  true
#define DOOR_SENSOR_GPIO 4
#define I2C_SDA_GPIO 2
#define I2C_SCL_GPIO 3
#define BH1750_ADDR 0x23

typedef struct {
    float lux;
    bool door_open;
    bool lock_on;
} device_status_t;

extern esp_rmaker_device_t *switch_device;
extern esp_rmaker_device_t *door_sensor_device;
extern esp_rmaker_param_t *door_status_param;
extern esp_rmaker_param_t *light_param;
extern esp_rmaker_param_t *system_status_param;

void app_driver_init(void);
int app_driver_set_state(bool state);
bool app_driver_get_state(void);
void app_sensor_init(void);
void report_all_status(void);
void update_system_status(void);
