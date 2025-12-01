#include "SK6812_RGB.hpp"
#include "../ambient_light_manager/ambient_light_manager.h"
#include "esp_timer.h"
#include "mt6701/mt6701_sensor.h"
#include "esp_log.h"


// #define LED_RING
#define LED_TRAIL
// #define ANDROID_EFFECT

#define NUM_LEDS     24

MT6701Sensor encoder = MT6701Sensor();
sk ledstrip;

#ifdef ANDROID_EFFECT

#define BASE_COLOR   0x00FF00
#define BACK_COLOR   0x000000
#define SPEED_DELAY  30         // ms per frame

void android_effect(uint8_t brightness)
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

        ledstrip.color32(i, lit ? BASE_COLOR : BACK_COLOR, brightness);
    }

    ledstrip.show();
    vTaskDelay(pdMS_TO_TICKS(10));
}
#endif // ANDROID_EFFECT


void status_led_manager_task(void *pvParameters)
{
    const int ledCount = NUM_LEDS;
    int head = 0;
    int prev = 0;

    while (1)
    {
        uint8_t lux_brightness = get_brightness_from_lux(get_average_lux());

#ifdef LED_RING
        // Clear all LEDs first
        // ledstrip.clear();        // clear() malfunctioning?
        ledstrip.color32(((head - 1) < 0) ? (head - 1) + ledCount : (head - 1),
                           ledstrip.getcolor(0, 0, 0));

        // Light up one LED (the "head")
        ledstrip.color32(head, ledstrip.getcolor(0, 255, 0), lux_brightness);
        
        ledstrip.show();
        
        // Move to next LED
        head = (head + 1) % ledCount;
        // ledstrip.color32(head, ledstrip.getcolor(0, 0, 0)); // red pixel

        vTaskDelay(50 / portTICK_PERIOD_MS); // adjust for speed
#endif
#ifdef LED_TRAIL
        // Define the fade steps as percentages of head brightness
        const int fadeSteps[6] = {100, 80, 60, 40, 20, 0}; // head to tail
            float encoderValue = encoder.getSensorAngle();  // 0-6
            // float encoderValue = 3.0;  // 0-6
            int index = ((float)encoderValue * 4);          // map 0-6 -> 0-24
            // int index = (encoderValue);          // map 0-6 -> 0-24

            // Turn off previous LED
            for (int u = 0; u < 24; u++){
                ledstrip.color(u, 0, 0, 0, 0);
            } // tail length = 5 LEDs + head
            ESP_LOGI("LED", "%f encoder ", encoderValue);
            // Turn on current LED
            ledstrip.color(index,
                        255 * lux_brightness / 100,  // red
                        0,
                        0,
                        lux_brightness);

            prev = index;            

        ledstrip.show();

        vTaskDelay(30 / portTICK_PERIOD_MS); // speed control
#endif
#ifdef ANDROID_EFFECT
        android_effect(lux_brightness);
#endif
    }

    vTaskDelete(NULL);
}

void status_led_manager_init(void)
{
    ledstrip.begin(GPIO_NUM_8, NUM_LEDS);
    ledstrip.setbrightness(50); // Set initial brightness to 50%
    encoder.init();
    encoder.update();    

    // Create the status LED manager task
    xTaskCreate(status_led_manager_task, "status_led_manager_task", 2048, NULL, 5, NULL);
}