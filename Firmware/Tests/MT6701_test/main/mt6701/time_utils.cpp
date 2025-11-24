#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "time_utils.h"

// ---------------------------------------------------------------------------
// delay(ms) replacement
// ---------------------------------------------------------------------------
void delay(unsigned long ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));   // FreeRTOS millisecond delay
}

// ---------------------------------------------------------------------------
// delayMicroseconds(us) replacement (optional)
// ---------------------------------------------------------------------------
void delayMicroseconds(unsigned long us) {
    esp_rom_delay_us(us);            // Busy-wait delay
}

// ---------------------------------------------------------------------------
// micros() replacement
// ---------------------------------------------------------------------------
unsigned long micros() {
    return (unsigned long) esp_timer_get_time();   // returns µs
}

// ---------------------------------------------------------------------------
// millis() replacement (optional)
// ---------------------------------------------------------------------------
unsigned long millis() {
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}

// function buffering delay() 
// arduino uno function doesn't work well with interrupts
void _delay(unsigned long ms){
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega328PB__)  || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega32U4__)
  // if arduino uno and other atmega328p chips
  // use while instad of delay, 
  // due to wrong measurement based on changed timer0
  unsigned long t = _micros() + ms*1000;
  while( _micros() < t ){}; 
#else
  // regular micros
  delay(ms);
#endif
}


// function buffering _micros() 
// arduino function doesn't work well with interrupts
unsigned long _micros(){
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega328PB__)  || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega32U4__)
// if arduino uno and other atmega328p chips
    //return the value based on the prescaler
    if((TCCR0B & 0b00000111) == 0x01) return (micros()/32);
    else return (micros());
#else
  // regular micros
  return micros();
#endif
}
