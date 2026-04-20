/* Switch Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_ota.h>

#include <esp_rmaker_common_events.h>

#include <app_network.h>
#include <app_insights.h>

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <math.h>
#include "app_priv.h"

static const char *TAG = "app_main";
esp_rmaker_device_t *switch_device;
esp_rmaker_device_t *door_sensor_device;
esp_rmaker_param_t *door_status_param;
esp_rmaker_param_t *light_param;
esp_rmaker_param_t *system_status_param;

static device_status_t g_current_status = {
    .lux = 0.0,
    .door_open = false,
    .lock_on = false
};

void update_system_status(void)
{
    if (!system_status_param) {
        return;
    }
    char status_str[110];
    g_current_status.lock_on = app_driver_get_state();
    
    snprintf(status_str, sizeof(status_str), "Door: [%s] ; Card: [%s] ; Lock: [%s]",
             g_current_status.door_open ? "Open" : "Closed",
             g_current_status.lux < 50 ? "In Case" : "Removed",
             g_current_status.lock_on ? "ON" : "OFF");

    ESP_LOGI(TAG, "System Update: %s", status_str);
    esp_rmaker_param_update_and_report(system_status_param, esp_rmaker_str(status_str));
}

void report_all_status(void)
{
    g_current_status.lock_on = app_driver_get_state();
    
    ESP_LOGI(TAG, "--- Smart Hook Status Report ---");
    ESP_LOGI(TAG, "Luminosity: %.2f Lux (%s)", g_current_status.lux, 
             g_current_status.lux < 50 ? "Card in slot" : "Card removed");
    ESP_LOGI(TAG, "Door Status: %s", g_current_status.door_open ? "OPEN" : "CLOSED");
    ESP_LOGI(TAG, "Lock Status: %s", g_current_status.lock_on ? "LOCKED" : "UNLOCKED");
    ESP_LOGI(TAG, "--------------------------------");

    /* Cloud Updates */
    if (light_param) {
        esp_rmaker_param_update_and_report(light_param, esp_rmaker_float(g_current_status.lux));
    }
    if (door_status_param) {
        esp_rmaker_param_update_and_report(door_status_param, esp_rmaker_bool(g_current_status.door_open));
    }
    
    /* Integrated Alert Update */
    update_system_status();
}

static void sensor_task(void *pvParameters)
{
    uint8_t data[2];
    float last_reported_lux = -100.0;
    bool last_door_open = false;
    bool last_lock_on = false;

    while (1) {
        bool status_changed = false;
        
        /* 1. Read GY-30 (BH1750) */
        esp_err_t err = i2c_master_read_from_device(I2C_NUM_0, BH1750_ADDR, data, 2, pdMS_TO_TICKS(100));
        if (err == ESP_OK) {
            float current_lux = (float)((data[0] << 8) | data[1]) / 1.2;
            g_current_status.lux = current_lux;
        }

        /* 2. Read IR Sensor (GPIO 4) 
         * Logic Correction: Low (0) -> Door Closed, High (1) -> Door Open
         */
        bool is_door_open = (gpio_get_level(DOOR_SENSOR_GPIO) == 1);
        g_current_status.door_open = is_door_open;

        /* 3. Get Lock State */
        bool current_lock_on = app_driver_get_state();
        g_current_status.lock_on = current_lock_on;

        /* 4. Core Safety Logic */
        if (is_door_open && g_current_status.lux < 50) {
            if (!current_lock_on) {
                ESP_LOGW(TAG, "SAFETY TRIGGER: Door open & Card detected. Locking servo!");
                app_driver_set_state(true);
                current_lock_on = true;
                g_current_status.lock_on = true;
                esp_rmaker_param_update_and_report(
                    esp_rmaker_device_get_param_by_name(switch_device, ESP_RMAKER_DEF_POWER_NAME),
                    esp_rmaker_bool(true));
                status_changed = true;
            }
        }

        /* 5. Reporting & Combined Alert Logic */
        bool card_present = (g_current_status.lux < 50);
        static bool last_card_present = false;
        
        bool card_changed = (card_present != last_card_present);
        bool door_changed = (is_door_open != last_door_open);
        bool lock_changed = (current_lock_on != last_lock_on);
        bool lux_significant = (fabs(g_current_status.lux - last_reported_lux) > 10.0);

        if (door_changed || card_changed || lock_changed || lux_significant) {
            status_changed = true;
        }

        if (status_changed) {
            report_all_status();
            
            /* Only raise a phone alert if it's a critical state change (not just small Lux drift) */
            if (door_changed || card_changed || lock_changed) {
                /* Extra check for MQTT/RainMaker ready state */
                if (switch_device) {
                    char alert_msg[128];
                    snprintf(alert_msg, sizeof(alert_msg), "Status Change! Door: %s, Card: %s, Lock: %s",
                             is_door_open ? "Open" : "Closed",
                             card_present ? "In Case" : "Removed",
                             current_lock_on ? "ON" : "OFF");
                    esp_rmaker_raise_alert(alert_msg);
                }
            }
            
            last_door_open = is_door_open;
            last_card_present = card_present;
            last_lock_on = current_lock_on;
            last_reported_lux = g_current_status.lux;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_sensor_init(void)
{
    /* I2C Init */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

    uint8_t cmd = 0x10; // Continuous H-Res Mode
    i2c_master_write_to_device(I2C_NUM_0, BH1750_ADDR, &cmd, 1, pdMS_TO_TICKS(100));

    /* GPIO Init */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pin_bit_mask = (1ULL << DOOR_SENSOR_GPIO)
    };
    gpio_config(&io_conf);

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    if (strcmp(esp_rmaker_param_get_name(param), ESP_RMAKER_DEF_POWER_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", esp_rmaker_device_get_name(device),
                esp_rmaker_param_get_name(param));
        app_driver_set_state(val.val.b);
        esp_rmaker_param_update(param, val);
    }
    return ESP_OK;
}
/* Event handler for catching RainMaker events */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == RMAKER_EVENT) {
        switch (event_id) {
            case RMAKER_EVENT_INIT_DONE:
                ESP_LOGI(TAG, "RainMaker Initialised.");
                break;
            case RMAKER_EVENT_CLAIM_STARTED:
                ESP_LOGI(TAG, "RainMaker Claim Started.");
                break;
            case RMAKER_EVENT_CLAIM_SUCCESSFUL:
                ESP_LOGI(TAG, "RainMaker Claim Successful.");
                break;
            case RMAKER_EVENT_CLAIM_FAILED:
                ESP_LOGI(TAG, "RainMaker Claim Failed.");
                break;
            case RMAKER_EVENT_LOCAL_CTRL_STARTED:
                ESP_LOGI(TAG, "Local Control Started.");
                break;
            case RMAKER_EVENT_LOCAL_CTRL_STOPPED:
                ESP_LOGI(TAG, "Local Control Stopped.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled RainMaker Event: %"PRIi32, event_id);
        }
    } else if (event_base == RMAKER_COMMON_EVENT) {
        switch (event_id) {
            case RMAKER_EVENT_REBOOT:
                ESP_LOGI(TAG, "Rebooting in %d seconds.", *((uint8_t *)event_data));
                break;
            case RMAKER_EVENT_WIFI_RESET:
                ESP_LOGI(TAG, "Wi-Fi credentials reset.");
                break;
            case RMAKER_EVENT_FACTORY_RESET:
                ESP_LOGI(TAG, "Node reset to factory defaults.");
                break;
            case RMAKER_MQTT_EVENT_CONNECTED:
                ESP_LOGI(TAG, "MQTT Connected.");
                break;
            case RMAKER_MQTT_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "MQTT Disconnected.");
                break;
            case RMAKER_MQTT_EVENT_PUBLISHED:
                ESP_LOGI(TAG, "MQTT Published. Msg id: %d.", *((int *)event_data));
                break;
            default:
                ESP_LOGW(TAG, "Unhandled RainMaker Common Event: %"PRIi32, event_id);
        }
    } else if (event_base == APP_NETWORK_EVENT) {
        switch (event_id) {
            case APP_NETWORK_EVENT_QR_DISPLAY:
                ESP_LOGI(TAG, "Provisioning QR : %s", (char *)event_data);
                break;
            case APP_NETWORK_EVENT_PROV_TIMEOUT:
                ESP_LOGI(TAG, "Provisioning Timed Out. Please reboot.");
                break;
            case APP_NETWORK_EVENT_PROV_RESTART:
                ESP_LOGI(TAG, "Provisioning has restarted due to failures.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled App Wi-Fi Event: %"PRIi32, event_id);
                break;
        }
    } else if (event_base == RMAKER_OTA_EVENT) {
        switch(event_id) {
            case RMAKER_OTA_EVENT_STARTING:
                ESP_LOGI(TAG, "Starting OTA.");
                break;
            case RMAKER_OTA_EVENT_IN_PROGRESS:
                ESP_LOGI(TAG, "OTA is in progress.");
                break;
            case RMAKER_OTA_EVENT_SUCCESSFUL:
                ESP_LOGI(TAG, "OTA successful.");
                break;
            case RMAKER_OTA_EVENT_FAILED:
                ESP_LOGI(TAG, "OTA Failed.");
                break;
            case RMAKER_OTA_EVENT_REJECTED:
                ESP_LOGI(TAG, "OTA Rejected.");
                break;
            case RMAKER_OTA_EVENT_DELAYED:
                ESP_LOGI(TAG, "OTA Delayed.");
                break;
            case RMAKER_OTA_EVENT_REQ_FOR_REBOOT:
                ESP_LOGI(TAG, "Firmware image downloaded. Please reboot your device to apply the upgrade.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled OTA Event: %"PRIi32, event_id);
                break;
        }
    } else {
        ESP_LOGW(TAG, "Invalid event received!");
    }
}

void app_main()
{
    /* Initialize Application specific hardware drivers and
     * set initial state.
     */
    esp_rmaker_console_init();
    app_driver_init();
    app_driver_set_state(DEFAULT_POWER);
    app_sensor_init();

    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    /* Initialize Wi-Fi. Note that, this should be called before esp_rmaker_node_init()
     */
    app_network_init();

    /* Register an event handler to catch RainMaker events */
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(APP_NETWORK_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_network_init() but before app_nenetworkk_start()
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Anti-Lock");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    /* Create a Anti-Lock device.
     * You can optionally use the helper API esp_rmaker_switch_device_create() to
     * avoid writing code for adding the name and power parameters.
     */
    switch_device = esp_rmaker_device_create("Anti-Lock", ESP_RMAKER_DEVICE_SWITCH, NULL);

    /* Add the write callback for the device. We aren't registering any read callback yet as
     * it is for future use.
     */
    esp_rmaker_device_add_cb(switch_device, write_cb, NULL);

    /* Add the standard name parameter (type: esp.param.name), which allows setting a persistent,
     * user friendly custom name from the phone apps. All devices are recommended to have this
     * parameter.
     */
    esp_rmaker_device_add_param(switch_device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "Anti-Lock"));

    /* Add the standard power parameter (type: esp.param.power), which adds a boolean param
     * with a toggle switch ui-type.
     */
    esp_rmaker_param_t *power_param = esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, DEFAULT_POWER);
    esp_rmaker_device_add_param(switch_device, power_param);

    /* Assign the power parameter as the primary, so that it can be controlled from the
     * home screen of the phone apps.
     */
    esp_rmaker_device_assign_primary_param(switch_device, power_param);

    /* Add this switch device to the node */
    esp_rmaker_node_add_device(node, switch_device);

    /* Create Door Sensor device */
    door_sensor_device = esp_rmaker_device_create("Door Sensor", "esp.device.contact-sensor", NULL);
    esp_rmaker_device_add_param(door_sensor_device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "Door Sensor"));
    
    door_status_param = esp_rmaker_param_create("Door Status", "esp.param.contact-state", esp_rmaker_bool(false), PROP_FLAG_READ);
    esp_rmaker_device_add_param(door_sensor_device, door_status_param);
    
    /* Add Ambient Light parameter to the Door Sensor device (or create a new device if preferred) */
    light_param = esp_rmaker_param_create("Ambient Light", "esp.param.luminosity", esp_rmaker_float(0.0), PROP_FLAG_READ);
    esp_rmaker_device_add_param(door_sensor_device, light_param);

    /* Add Integrated System Status parameter */
    system_status_param = esp_rmaker_param_create("System Status", "esp.param.name", esp_rmaker_str("Initializing..."), PROP_FLAG_READ);
    esp_rmaker_device_add_param(door_sensor_device, system_status_param);

    esp_rmaker_node_add_device(node, door_sensor_device);

    /* Enable OTA */
    esp_rmaker_ota_enable_default();

    /* Enable timezone service which will be require for setting appropriate timezone
     * from the phone apps for scheduling to work correctly.
     * For more information on the various ways of setting timezone, please check
     * https://rainmaker.espressif.com/docs/time-service.html.
     */
    esp_rmaker_timezone_service_enable();

    /* Enable scheduling. */
    esp_rmaker_schedule_enable();

    /* Enable Scenes */
    esp_rmaker_scenes_enable();

    /* Enable Insights. Requires CONFIG_ESP_INSIGHTS_ENABLED=y */
    app_insights_enable();

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    err = app_network_set_custom_mfg_data(MFG_DATA_DEVICE_TYPE_SWITCH, MFG_DATA_DEVICE_SUBTYPE_SWITCH);
    /* Start the Wi-Fi.
     * If the node is provisioned, it will start connection attempts,
     * else, it will start Wi-Fi provisioning. The function will return
     * after a connection has been successfully established
     */
    err = app_network_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }
}
