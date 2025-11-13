#ifndef SK_H
#define SK_H

#include "driver/rmt.h"

// Use RMT for a strip of SK6812 RGBW leds

#ifdef __cplusplus
extern "C" {
#endif

#define RGB_DATA_WIDTH 24 // 3 bytes per led

class sk
{
public:
    sk(void) {}
    ~sk(void)
    {

        free(_skstrip);
        rmt_driver_uninstall(skconfig.channel);
    }

    /*-------------------------------------------------------------------------*/
    int begin(int pin, int count)
    {

        _bitcount = count * RGB_DATA_WIDTH;
        _ledpin = pin;
        _ledcount = count;
        _brightness = 100;

        int rc;

        _skstrip = (rmt_item32_t *)calloc(_bitcount, sizeof(rmt_item32_t));

        skconfig.rmt_mode = RMT_MODE_TX;
        skconfig.channel = RMT_CHANNEL_0;
        skconfig.gpio_num = (gpio_num_t)_ledpin;
        skconfig.mem_block_num = 1;
        skconfig.tx_config.loop_en = 0;
        skconfig.tx_config.carrier_en = 0;
        skconfig.tx_config.idle_output_en = 1;
        skconfig.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
        skconfig.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
        skconfig.clk_div = 8; // 80MHx / 8 = 10MHz translates to  0,1 us = 100 ns per count

        rmt_config(&skconfig);
        rc = rmt_driver_install(skconfig.channel, 0, 0); //  rmt_driver_install(rmt_channel_t channel, size_t rx_buf_size, int rmt_intr_num)

        return (rc);
    }

    /*-------------------------------------------------------------------------*/
    uint32_t getcolor(uint8_t r, uint8_t g, uint8_t b)
    {
        uint32_t kleur = 0;
        kleur |= ((uint32_t)g << 16);
        kleur |= ((uint32_t)r << 8);
        kleur |= (uint32_t)b;
        return kleur;
    }

    /*-------------------------------------------------------------------------*/
    uint32_t breakcolor(uint32_t kleur, uint8_t *r, uint8_t *g, uint8_t *b)
    {
        *g = (kleur >> 16) & 0xff;
        *r = (kleur >> 8) & 0xff;
        *b = kleur & 0xff;
        return kleur;
    }

    /*-------------------------------------------------------------------------*/
    void color32(int led, uint32_t kleur, int brightness = -1)
    {
        uint8_t r, g, b;

        breakcolor(kleur, &r, &g, &b);
        color(led%_ledcount, r, g, b, brightness);
    }

    /*-------------------------------------------------------------------------*/
    void color(int led, uint8_t red, uint8_t green, uint8_t blue, int brightness = -1)
    {
        uint32_t kleur = 0, bright;
        uint8_t r = red, g = green, b = blue;
        int i, bit;

        if (led < 0 || led >= ledcount())
            return;

        // Serial.printf( "---\nrgb = %d.%d.%d\n", red,green,blue);
        //  brightness is a percentage 0...100

        bright = brightness;
        if (brightness == -1)
            bright = 100;

        bright = (bright * _brightness) / 100;

        r = bright * red / 100;
        g = bright * green / 100;
        b = bright * blue / 100;

        // Serial.printf("_brightness = %d, brightness %d, bright %d\n", _brightness, brightness, bright);
        // Serial.printf( "rgb = %d.%d.%d\n---\n", r,g,b);

        kleur |= ((uint32_t)g << 16);
        kleur |= ((uint32_t)r << 8);
        kleur |= (uint32_t)b;

        // Serial.printf("Set color of led %d kleur %08X\n", led, kleur);
        //  sk6812 has around 600us/600us 1, 300/900us 0

        for (i = (led * RGB_DATA_WIDTH), bit = 0; bit < RGB_DATA_WIDTH; bit++)
        {

            if ((kleur & (1 << ((RGB_DATA_WIDTH-1) - bit))))
            {
                _skstrip[i].level0 = 1;
                _skstrip[i].duration0 = 6;
                _skstrip[i].level1 = 0;
                _skstrip[i].duration1 = 6;
            }
            else
            {
                _skstrip[i].level0 = 1;
                _skstrip[i].duration0 = 3;
                _skstrip[i].level1 = 0;
                _skstrip[i].duration1 = 9;
            }
            // if ( bit == 31 )  _skstrip[i].duration1 += 60;
            ++i;
        }

        return;
    }

    /*-------------------------------------------------------------------------*/
    void show()
    {

        int rc;
        // esp_err_t rmt_write_items(rmt_channel_t channel, rmt_item32_t *rmt_item, int item_num, bool wait_tx_done)

        rc = rmt_write_items(skconfig.channel, _skstrip, _bitcount, 1);

        vTaskDelay(2 / portTICK_PERIOD_MS);
    }

    /*-------------------------------------------------------------------------*/

    void clear(void)
    {

        for (int i = 0; i < _ledcount; ++i)
        {
            color(i, 0, 0, 0);
        }

        show();
        vTaskDelay(2 / portTICK_PERIOD_MS);
    }

    /*-------------------------------------------------------------------------*/
    uint8_t setbrightness(uint8_t newbrightness)
    {
        _brightness = newbrightness;
        return (_brightness);
    }
    /*-------------------------------------------------------------------------*/
    uint8_t getbrightness()
    {
        return (_brightness);
    }
    /*-------------------------------------------------------------------------*/

    int ledcount()
    {
        return (_ledcount);
    }

    /*-------------------------------------------------------------------------*/

private:
    int _ledcount;
    int _bitcount;
    int _ledpin;
    int _brightness;
    rmt_item32_t *_skstrip = NULL;
    rmt_config_t skconfig;
    float _realtick;
};

#ifdef __cplusplus
}
#endif

#endif