#include <cmath>   
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "mt6701_sensor.h"
#include "time_utils.h"

static const float ALPHA = 0.4;

#define SENSOR_MT6701 1
#define PIN_MT_CSN 21
#define PIN_MT_CLOCK 22
#define PIN_MT_DATA 23


static uint8_t tableCRC6[64] = {
 0x00, 0x03, 0x06, 0x05, 0x0C, 0x0F, 0x0A, 0x09,
 0x18, 0x1B, 0x1E, 0x1D, 0x14, 0x17, 0x12, 0x11,
 0x30, 0x33, 0x36, 0x35, 0x3C, 0x3F, 0x3A, 0x39,
 0x28, 0x2B, 0x2E, 0x2D, 0x24, 0x27, 0x22, 0x21,
 0x23, 0x20, 0x25, 0x26, 0x2F, 0x2C, 0x29, 0x2A,
 0x3B, 0x38, 0x3D, 0x3E, 0x37, 0x34, 0x31, 0x32,
 0x13, 0x10, 0x15, 0x16, 0x1F, 0x1C, 0x19, 0x1A,
 0x0B, 0x08, 0x0D, 0x0E, 0x07, 0x04, 0x01, 0x02
};

/*32-bit input data, right alignment, Calculation over 18 bits (mult. of 6) */
static uint8_t CRC6_43_18bit (uint32_t w_InputData)
{
 uint8_t b_Index = 0;
 uint8_t b_CRC = 0;

 b_Index = (uint8_t )(((uint32_t)w_InputData >> 12u) & 0x0000003Fu);

 b_CRC = (uint8_t )(((uint32_t)w_InputData >> 6u) & 0x0000003Fu);
 b_Index = b_CRC ^ tableCRC6[b_Index];

 b_CRC = (uint8_t )((uint32_t)w_InputData & 0x0000003Fu);
 b_Index = b_CRC ^ tableCRC6[b_Index];

 b_CRC = tableCRC6[b_Index];

 return b_CRC;
} 

#if SENSOR_MT6701

MT6701Sensor::MT6701Sensor() {}

void MT6701Sensor::init() {

    // pinMode(PIN_MT_CSN, OUTPUT);
    // digitalWrite(PIN_MT_CSN, HIGH);

    // Configure CSN pin as output
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = 1ULL << PIN_MT_CSN;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);

  // Set pin HIGH
  gpio_set_level((gpio_num_t)PIN_MT_CSN, 1);

  spi_bus_config_t tx_bus_config = {
      .mosi_io_num = -1,
      .miso_io_num = PIN_MT_DATA,
      .sclk_io_num = PIN_MT_CLOCK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 1000,
  };
  
  #ifdef CONFIG_IDF_TARGET_ESP32S3
    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &tx_bus_config, SPI_DMA_CH_AUTO);
  #else
    // esp_err_t ret = spi_bus_initialize(HSPI_HOST, &tx_bus_config, 1);
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &tx_bus_config, SPI_DMA_CH_AUTO);
  #endif
  
  ESP_ERROR_CHECK(ret);

  spi_device_interface_config_t tx_device_config = {
      .command_bits=0,
      .address_bits=0,
      .dummy_bits=0,
      .mode=1,
      .clock_source=SPI_CLK_SRC_XTAL,
      .duty_cycle_pos=0,
      .cs_ena_pretrans=4,
      .cs_ena_posttrans=0,
      .clock_speed_hz=4000000,
      .input_delay_ns=0,
      .spics_io_num=PIN_MT_CSN,
      .flags = 0,
      .queue_size=1,
      .pre_cb=NULL,
      .post_cb=NULL,
  };
  #ifdef CONFIG_IDF_TARGET_ESP32S3
    ret=spi_bus_add_device(SPI3_HOST, &tx_device_config, &spi_device_);
  #else
    // ret=spi_bus_add_device(HSPI_HOST, &tx_device_config, &spi_device_);
    ret=spi_bus_add_device(SPI2_HOST, &tx_device_config, &spi_device_);
  #endif
  
  ESP_ERROR_CHECK(ret);


  spi_transaction_.flags = SPI_TRANS_USE_RXDATA;
  spi_transaction_.length = 24;
  spi_transaction_.rxlength = 24;
  spi_transaction_.tx_buffer = NULL;
  spi_transaction_.rx_buffer = NULL;
}

float MT6701Sensor::getSensorAngle() {
    uint32_t now = micros();
    if (now - last_update_ > 100) {
      
      esp_err_t ret=spi_device_polling_transmit(spi_device_, &spi_transaction_);
      assert(ret==ESP_OK);

      uint32_t spi_32 = (spi_transaction_.rx_data[0] << 16) | (spi_transaction_.rx_data[1] << 8) | spi_transaction_.rx_data[2];
      uint32_t angle_spi = spi_32 >> 10;

      uint8_t field_status = (spi_32 >> 6) & 0x3;
      uint8_t push_status = (spi_32 >> 8) & 0x1;
      uint8_t loss_status = (spi_32 >> 9) & 0x1;

      uint8_t received_crc = spi_32 & 0x3F;
      uint8_t calculated_crc = CRC6_43_18bit(spi_32 >> 6);
      
      if (received_crc == calculated_crc) {
        float new_angle = (float)angle_spi * 2 * PI / 16384;
        float new_x = cosf(new_angle);
        float new_y = sinf(new_angle);
        x_ = new_x * ALPHA + x_ * (1-ALPHA);
        y_ = new_y * ALPHA + y_ * (1-ALPHA);
      } else {
        error_ = {
          .error = true,
          .received_crc = received_crc,
          .calculated_crc = calculated_crc,
        };
      }

      last_update_ = now;
    }
    float rad = -atan2f(y_, x_);
    if (rad < 0) {
        rad += 2*PI;
    }
    return rad;
}

MT6701Error MT6701Sensor::getAndClearError() {
  MT6701Error out = error_;
  error_ = {};
  return out;
}

#endif