#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define LUX_BUFFER_SIZE 10
#define MESSAGE_PRINT_DELAY 1000

void ambient_sensor_init(void);
float get_current_lux(void);
float get_average_lux(void);
uint8_t get_brightness_from_lux(float lux);

#ifdef __cplusplus
}
#endif