/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <cmath>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "status_led_driver/SK6812_RGB.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LED_RING_V2
// #define LED_RING
// #define LED_TRAIL
// #define ANDROID_EFFECT

#define NUM_LEDS     24

sk ledstrip;

void print_chip_info(void)
{
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
}

#ifdef ANDROID_EFFECT

#define BASE_COLOR   0x00FF00
#define BACK_COLOR   0x000000
#define SPEED_DELAY  60         // ms per frame

void android_effect()
{
    static uint32_t lastUpdate = 0;
    static uint8_t startIndex = 0; // Start position of the bright segment
    static uint8_t size = 2;       // Length of the bright segment
    static bool shrinking = false;
    static uint8_t counter = 0;

    uint32_t now = esp_timer_get_time()/1000; // Convert to milliseconds

    if (now - lastUpdate > SPEED_DELAY)
    {
        lastUpdate = now;
        counter++;

        // Change segment size dynamically (grow/shrink)
        if (!shrinking)
        {
            if (counter % 3 == 1)
                startIndex++;
            else
                size++;
            if (size >= NUM_LEDS / 2) // limit maximum growth
                shrinking = true;
        }
        else
        {
            startIndex++;
            if (counter % 3 != 1)
                size--;
            if (size <= 1)
                shrinking = false;
        }

        if (startIndex >= NUM_LEDS)
            startIndex = 0;
    }

    // --- Draw the animation ---
    uint8_t endIndex = (startIndex + size) % NUM_LEDS;

    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        bool lit = false;
        if (startIndex < endIndex)
            lit = (i >= startIndex && i < endIndex);
        else
            lit = (i >= startIndex || i < endIndex);

        ledstrip.color32(i, lit ? BASE_COLOR : BACK_COLOR, 2);
    }

    ledstrip.show();
    vTaskDelay(pdMS_TO_TICKS(10));
}
#endif // ANDROID_EFFECT

void led_task(void *pvParameters);

void app_main(void)
{
    print_chip_info();
    int ledCount = NUM_LEDS;

    ledstrip.begin(GPIO_NUM_8, ledCount); // pin, number of leds

    int head = 11;

#ifdef LED_RING_V2
    xTaskCreate(
        led_task,          // Task function
        "LED_Task",        // Task name
        2048,              // Stack size (bytes or words depending on port)
        NULL,              // Parameter
        5,                 // Priority
        NULL               // Task handle (not needed)
    );
#endif

#ifdef LED_RING
    while (1)
    {
        // Clear all LEDs first
        // ledstrip.clear();        // clear() malfunctioning?
        ledstrip.color32(((head - 1) < 0) ? (head - 1) + ledCount : (head - 1),
                           ledstrip.getcolor(0, 0, 0));

        // Light up one LED (the "head")
        ledstrip.color32(head, ledstrip.getcolor(0, 255, 0), 100);
        
        ledstrip.show();
        
        // Move to next LED
        head = (head + 1) % ledCount;
        // ledstrip.color32(head, ledstrip.getcolor(0, 0, 0)); // red pixel

        vTaskDelay(50 / portTICK_PERIOD_MS); // adjust for speed
    }
#endif
#ifdef LED_TRAIL
    while (1)
    {
        // ledstrip.clear();

        for (int t = 0; t < 6; t++) // tail length = 5 LEDs
        {
            int index = (head - t + ledCount) % ledCount; // wrap around ring
            int brightness = 100 - t * 20;          // fade trail (100%, 80%, 60%, 40%, 20%)
            if (brightness < 0) brightness = 0;

            ledstrip.color(index,
                           255 * brightness / 100,
                           0,
                           0,
                           brightness);
        }
        ledstrip.color(head+1, 255 * .10, 0, 0, 10);

        ledstrip.show();
        head = (head + 1) % ledCount;

        vTaskDelay(50 / portTICK_PERIOD_MS); // speed control
    }
#endif
#ifdef ANDROID_EFFECT
    while(1)
    {
        android_effect();
    }
#endif

}

#ifdef LED_RING_V2
#define LED_FPS          60    
#define FRAME_TIME_MS   (1000 / LED_FPS)  // ~30 FPS
#define LED_SPEED_LPS     24
int head = 0;
int ledCount = NUM_LEDS;

void led_task(void *pvParameters)
{
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t start_time = xTaskGetTickCount();

    while (1)
    {
        uint32_t elapsed_ms = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;

        float pos = fmodf((elapsed_ms / 1000.0f) * LED_SPEED_LPS, (float)ledCount);

        int head_i = (int)pos;
        int prev_i = (head_i == 0) ? (ledCount - 1) : (head_i - 1);

        for (int i = 0; i < ledCount; i++) {
            ledstrip.color32(i, ledstrip.getcolor(0, 0, 0), 0);
        }

        ledstrip.color32(head_i, ledstrip.getcolor(0, 255, 0), 100);

        ledstrip.show();

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(FRAME_TIME_MS));
    }
}
#endif

#ifdef __cplusplus
}
#endif
