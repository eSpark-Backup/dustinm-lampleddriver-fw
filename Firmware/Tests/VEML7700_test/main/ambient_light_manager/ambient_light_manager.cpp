#include "driver/i2c_types.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#include "VEML7700/Adafruit_VEML7700.h"
#include "ambient_light_manager.h"

const char* TAG = "AMBIENT_LIGHT_SENSOR";

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float buffer[LUX_BUFFER_SIZE];
    size_t index;   // next write index
    size_t count;   // number of valid values stored
} LUX_RINGBUFFER_s;

Adafruit_VEML7700 ambientSensor = Adafruit_VEML7700();
i2c_master_dev_handle_t dev_handle = NULL;
LUX_RINGBUFFER_s luxValue;

// Initialize the ring buffer
static void luxRingBufferInit(LUX_RINGBUFFER_s *rb) {
    for (size_t i = 0; i < LUX_BUFFER_SIZE; i++) {
        rb->buffer[i] = 0.0f;
    }
    rb->index = 0;
    rb->count = 0;
}

// Push a new lux value into the ring buffer
static void luxRingBufferPush(LUX_RINGBUFFER_s *rb, float value) {
    rb->buffer[rb->index] = value;
    rb->index = (rb->index + 1) % LUX_BUFFER_SIZE;
    if (rb->count < LUX_BUFFER_SIZE) {
        rb->count++;
    }
}

// Peek the most recent value
static float luxRingBufferPeek(LUX_RINGBUFFER_s *rb) {
    if (rb->count == 0) return 0.0f;
    size_t lastIndex = (rb->index + LUX_BUFFER_SIZE - 1) % LUX_BUFFER_SIZE;
    return rb->buffer[lastIndex];
}

// Compute average of all stored values
static float luxRingBufferAverage(LUX_RINGBUFFER_s *rb) {
    if (rb->count == 0) return 0.0f;

    float sum = 0.0f;
    for (size_t i = 0; i < rb->count; i++) {
        sum += rb->buffer[i];
    }
    return sum / rb->count;
}
// ===========================================

static esp_err_t i2c_master_init()
{
    i2c_master_bus_config_t i2c_mst_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_6,
        .scl_io_num = GPIO_NUM_7,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .flags = {
            .enable_internal_pullup = false,
            .allow_pd = false,
        },
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = VEML7700_I2CADDR_DEFAULT,
        .scl_speed_hz = 100000,
    };

    return i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
}

static void ambient_light_task(void *pvParameters)
{
    static uint32_t lastMessageTimestamp = esp_timer_get_time() / 1000;
    while (1) {
        uint32_t now = esp_timer_get_time()/1000; // Convert to milliseconds
        luxRingBufferPush(&luxValue, ambientSensor.readLux(VEML_LUX_CORRECTED));
        if(now - lastMessageTimestamp >= MESSAGE_PRINT_DELAY) {
            lastMessageTimestamp = now;
            ESP_LOGI(TAG, " %.2f lux", get_average_lux());
        }
        vTaskDelay(pdMS_TO_TICKS(5)); // Delay for 1 second
    }

    vTaskDelete(NULL);
}

void ambient_sensor_init(void)
{
    esp_err_t err;
    
    err = i2c_master_init();
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Master initialization failed: %s", esp_err_to_name(err));
        return;
    }

    if (!ambientSensor.begin(dev_handle)) {
        ESP_LOGE(TAG, "Failed to find VEML7700 chip");
        return;
    }

    luxRingBufferInit(&luxValue);

    // Configure sensor settings as needed
    ambientSensor.setGain(VEML7700_GAIN_1_8); // Set gain
    ambientSensor.setIntegrationTime(VEML7700_IT_100MS); // Set integration time

    // create task for auto reading ambient light
    xTaskCreate(ambient_light_task, "ambient_light_task", 2048, NULL, 5, NULL);
}

float get_current_lux(void)
{
    return luxRingBufferPeek(&luxValue);
}

float get_average_lux(void)
{
    return luxRingBufferAverage(&luxValue);
}

uint8_t get_brightness_from_lux(float lux)
{
    const float LUX_MIN = 10.0f;
    const float LUX_MAX = 2000.0f;
    const uint8_t BRIGHT_MIN = 10;
    const uint8_t BRIGHT_MAX = 100;

    if (lux <= LUX_MIN) {
        return BRIGHT_MIN;
    } 
    else if (lux >= LUX_MAX) {
        return BRIGHT_MAX;
    } 
    else {
        // Scale brightness linearly between BRIGHT_MIN and BRIGHT_MAX
        float scale = (lux - LUX_MIN) / (LUX_MAX - LUX_MIN);
        return (uint8_t)(BRIGHT_MIN + scale * (BRIGHT_MAX - BRIGHT_MIN));
    }
}

#ifdef __cplusplus
}
#endif