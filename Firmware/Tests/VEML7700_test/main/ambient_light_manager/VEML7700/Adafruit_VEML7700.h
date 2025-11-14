#ifndef _ADAFRUIT_VEML7700_H
#define _ADAFRUIT_VEML7700_H

#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "esp_log.h"

#define VEML7700_I2CADDR_DEFAULT   0x10

// Register addresses
#define VEML7700_ALS_CONFIG        0x00
#define VEML7700_ALS_THREHOLD_HIGH 0x01
#define VEML7700_ALS_THREHOLD_LOW  0x02
#define VEML7700_ALS_POWER_SAVE    0x03
#define VEML7700_ALS_DATA          0x04
#define VEML7700_WHITE_DATA        0x05
#define VEML7700_INTERRUPTSTATUS   0x06

// Integration Time values
#define VEML7700_IT_25MS   0x0C
#define VEML7700_IT_50MS   0x08
#define VEML7700_IT_100MS  0x00
#define VEML7700_IT_200MS  0x01
#define VEML7700_IT_400MS  0x02
#define VEML7700_IT_800MS  0x03

// Gain
#define VEML7700_GAIN_1_8 0x00
#define VEML7700_GAIN_1_4 0x01
#define VEML7700_GAIN_1   0x02
#define VEML7700_GAIN_2   0x03

typedef enum {
    VEML_LUX_NORMAL,
    VEML_LUX_NORMAL_NOWAIT,
    VEML_LUX_CORRECTED,
    VEML_LUX_CORRECTED_NOWAIT,
    VEML_LUX_AUTO
} luxMethod;

#ifdef __cplusplus
extern "C" {
#endif

class Adafruit_VEML7700
{
public:
    Adafruit_VEML7700();
    bool begin(i2c_master_dev_handle_t dev);

    void enable(bool en);
    bool enabled();

    void interruptEnable(bool en);
    bool interruptEnabled();

    void setPersistence(uint8_t p);
    uint8_t getPersistence();

    void setIntegrationTime(uint8_t it, bool wait=true);
    uint8_t getIntegrationTime();
    int getIntegrationTimeValue();

    void setGain(uint8_t gain);
    uint8_t getGain();
    float getGainValue();

    void powerSaveEnable(bool en);
    bool powerSaveEnabled();

    void setPowerSaveMode(uint8_t mode);
    uint8_t getPowerSaveMode();

    void setLowThreshold(uint16_t v);
    uint16_t getLowThreshold();

    void setHighThreshold(uint16_t v);
    uint16_t getHighThreshold();

    uint16_t interruptStatus();

    uint16_t readALS(bool wait=false);
    uint16_t readWhite(bool wait=false);

    float readLux(luxMethod method=VEML_LUX_NORMAL);

private:
    float getResolution();
    float computeLux(uint16_t raw, bool corrected=false);
    float autoLux();
    void readWait();

    uint16_t readReg16(uint8_t reg);
    void writeReg16(uint8_t reg, uint16_t value);
    void modifyRegBits(uint8_t reg, uint16_t mask, uint16_t value);

    i2c_master_dev_handle_t dev;
    uint64_t lastRead;

    const float MAX_RES = 0.0036;
    const float GAIN_MAX = 2;
    const float IT_MAX = 800;
};

#ifdef __cplusplus
}
#endif

#endif
