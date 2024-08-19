#include <pthread.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_config.h"
#include "global_state.h"

#include "influx_task.h"

static const char * TAG = "influx_task";

static Influx * influxdb = 0;



// Timer callback function to increment uptime counters
void uptime_timer_callback(TimerHandle_t xTimer)
{
    // Increment uptime counters
    pthread_mutex_lock(&influxdb->lock);
    influxdb->stats.total_uptime += 1;
    influxdb->stats.uptime += 1;
    pthread_mutex_unlock(&influxdb->lock);
}

void influx_task_set_temperature(float temp, float temp2)
{
    if (!influxdb) {
        return;
    }
    pthread_mutex_lock(&influxdb->lock);
    influxdb->stats.temp = temp;
    influxdb->stats.temp2 = temp2;
    pthread_mutex_unlock(&influxdb->lock);
}

void influx_task_set_hashrate(float hashrate)
{
    if (!influxdb) {
        return;
    }
    pthread_mutex_lock(&influxdb->lock);
    influxdb->stats.hashing_speed = hashrate;
    pthread_mutex_unlock(&influxdb->lock);
}

static void forever() {
    ESP_LOGE(TAG, "halting influx_task");
    while (1) {
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }
}

void * influx_task(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;
    SystemModule * module = &GLOBAL_STATE->SYSTEM_MODULE;

    int influxEnable = nvs_config_get_u16(NVS_CONFIG_INFLUX_ENABLE, CONFIG_INFLUX_ENABLE);

    if (!influxEnable) {
        ESP_LOGI(TAG, "InfluxDB is not enabled.");
        forever();
    }

    char * influxURL = nvs_config_get_string(NVS_CONFIG_INFLUX_URL, CONFIG_INFLUX_URL);
    int influxPort = nvs_config_get_u16(NVS_CONFIG_INFLUX_PORT, CONFIG_INFLUX_PORT);
    char * influxToken = nvs_config_get_string(NVS_CONFIG_INFLUX_TOKEN, CONFIG_INFLUX_TOKEN);
    char * influxBucket = nvs_config_get_string(NVS_CONFIG_INFLUX_BUCKET, CONFIG_INFLUX_BUCKET);
    char * influxOrg = nvs_config_get_string(NVS_CONFIG_INFLUX_ORG, CONFIG_INFLUX_ORG);
    char * influxPrefix = nvs_config_get_string(NVS_CONFIG_INFLUX_PREFIX, CONFIG_INFLUX_PREFIX);

    ESP_LOGI(TAG, "URL: %s, port: %d, bucket: %s, org: %s, prefix: %s", influxURL, influxPort, influxBucket, influxOrg, influxPrefix);

    influxdb = influx_init(influxURL, influxPort, influxToken, influxBucket, influxOrg, influxPrefix);

    bool ping_ok = false;
    bool bucket_ok = false;
    bool loaded_values_ok = false;
    while (1) {
        do {
            ping_ok = ping_ok || influx_ping(influxdb);
            if (!ping_ok) {
                ESP_LOGE(TAG, "InfluxDB not reachable!");
                break;
            }

            bucket_ok = bucket_ok || bucket_exists(influxdb);
            if (!bucket_ok) {
                ESP_LOGE(TAG, "Bucket not found!");
                break;
            }

            loaded_values_ok = loaded_values_ok || load_last_values(influxdb);
            if (!loaded_values_ok) {
                ESP_LOGE(TAG, "loading last values failed");
                break;
            }
        } while(0);
        if (loaded_values_ok) {
            break;
        }
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "last values: total_uptime: %d, total_best_difficulty: %.3f, total_blocks_found: %d",
        influxdb->stats.total_uptime, influxdb->stats.total_best_difficulty, influxdb->stats.total_blocks_found);

    // start submitting new data
    ESP_LOGI(TAG, "waiting for clock sync ...");
    while (!module->lastClockSync) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "waiting for clock sync ... done");

    // Create and start the uptime timer with a 1-second period
    TimerHandle_t uptime_timer = xTimerCreate("UptimeTimer", pdMS_TO_TICKS(1000), pdTRUE, (void *) 0, uptime_timer_callback);
    if (uptime_timer != NULL) {
        xTimerStart(uptime_timer, 0);
    } else {
        ESP_LOGE(TAG, "Failed to create uptime timer");
        forever();
    }


    while (1) {
        pthread_mutex_lock(&influxdb->lock);
        influx_write(influxdb);
        pthread_mutex_unlock(&influxdb->lock);
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }
}