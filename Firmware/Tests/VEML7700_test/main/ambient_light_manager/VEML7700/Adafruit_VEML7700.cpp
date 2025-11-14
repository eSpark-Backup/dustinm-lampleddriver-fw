#include "Adafruit_VEML7700.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAG "VEML7700"

// -------------------- I2C helper functions --------------------

uint16_t Adafruit_VEML7700::readReg16(uint8_t reg)
{
    uint8_t rx[2];
    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev, &reg, 1, rx, 2, 100));
    return (uint16_t)rx[0] | ((uint16_t)rx[1] << 8);
}

void Adafruit_VEML7700::writeReg16(uint8_t reg, uint16_t value)
{
    uint8_t data[3] = {reg, (uint8_t)(value & 0xFF), (uint8_t)(value >> 8)};
    ESP_ERROR_CHECK(i2c_master_transmit(dev, data, 3, 100));
}

void Adafruit_VEML7700::modifyRegBits(uint8_t reg, uint16_t mask, uint16_t value)
{
    uint16_t regVal = readReg16(reg);
    regVal = (regVal & ~mask) | (value & mask);
    writeReg16(reg, regVal);
}

// -------------------- Constructor --------------------

Adafruit_VEML7700::Adafruit_VEML7700()
{
    dev = nullptr;
    lastRead = 0;
}

// -------------------- Begin --------------------

bool Adafruit_VEML7700::begin(i2c_master_dev_handle_t handle)
{
    dev = handle;

    // Try to read config register
    uint16_t cfg;
    esp_err_t err = i2c_master_transmit_receive(
        dev, (uint8_t[]){VEML7700_ALS_CONFIG}, 1, (uint8_t*)&cfg, 2, 100
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to communicate with VEML7700");
        return false;
    }

    enable(false);
    interruptEnable(false);
    setPersistence(0);
    setGain(VEML7700_GAIN_1_8);
    setIntegrationTime(VEML7700_IT_100MS);
    powerSaveEnable(false);
    enable(true);

    lastRead = esp_timer_get_time()/1000;

    return true;
}

// -------------------- Enable/Disable --------------------

void Adafruit_VEML7700::enable(bool en)
{
    modifyRegBits(VEML7700_ALS_CONFIG, 0x0001, en ? 0 : 1);

    if (en)
        vTaskDelay(pdMS_TO_TICKS(5));
}

bool Adafruit_VEML7700::enabled()
{
    return !(readReg16(VEML7700_ALS_CONFIG) & 0x0001);
}

// -------------------- Interrupt --------------------

void Adafruit_VEML7700::interruptEnable(bool en)
{
    modifyRegBits(VEML7700_ALS_CONFIG, 0x0002, en ? 0x0002 : 0);
}

bool Adafruit_VEML7700::interruptEnabled()
{
    return readReg16(VEML7700_ALS_CONFIG) & 0x0002;
}

// -------------------- Persistence --------------------

void Adafruit_VEML7700::setPersistence(uint8_t p)
{
    modifyRegBits(VEML7700_ALS_CONFIG, 0x0030, (p << 4));
}

uint8_t Adafruit_VEML7700::getPersistence()
{
    return (readReg16(VEML7700_ALS_CONFIG) >> 4) & 0x03;
}

// -------------------- Integration Time --------------------

void Adafruit_VEML7700::setIntegrationTime(uint8_t it, bool wait)
{
    int flush = wait ? getIntegrationTimeValue() : 0;

    modifyRegBits(VEML7700_ALS_CONFIG, 0x00C0, (it << 6));

    if (wait)
        vTaskDelay(pdMS_TO_TICKS(flush));

    lastRead = esp_timer_get_time() / 1000;
}

uint8_t Adafruit_VEML7700::getIntegrationTime()
{
    return (readReg16(VEML7700_ALS_CONFIG) >> 6) & 0x0F;
}

int Adafruit_VEML7700::getIntegrationTimeValue()
{
    switch (getIntegrationTime()) {
        case VEML7700_IT_25MS: return 25;
        case VEML7700_IT_50MS: return 50;
        case VEML7700_IT_100MS: return 100;
        case VEML7700_IT_200MS: return 200;
        case VEML7700_IT_400MS: return 400;
        case VEML7700_IT_800MS: return 800;
        default: return 100;
    }
}

// -------------------- Gain --------------------

void Adafruit_VEML7700::setGain(uint8_t gain)
{
    modifyRegBits(VEML7700_ALS_CONFIG, 0x1800, (gain << 11));
    lastRead = esp_timer_get_time()/1000;
}

uint8_t Adafruit_VEML7700::getGain()
{
    return (readReg16(VEML7700_ALS_CONFIG) >> 11) & 0x03;
}

float Adafruit_VEML7700::getGainValue()
{
    switch (getGain()) {
        case VEML7700_GAIN_1_8: return 0.125;
        case VEML7700_GAIN_1_4: return 0.25;
        case VEML7700_GAIN_1:   return 1;
        case VEML7700_GAIN_2:   return 2;
        default: return 1;
    }
}

// -------------------- Power Save --------------------

void Adafruit_VEML7700::powerSaveEnable(bool en)
{
    modifyRegBits(VEML7700_ALS_POWER_SAVE, 0x0001, en ? 1 : 0);
}

bool Adafruit_VEML7700::powerSaveEnabled()
{
    return readReg16(VEML7700_ALS_POWER_SAVE) & 0x01;
}

void Adafruit_VEML7700::setPowerSaveMode(uint8_t mode)
{
    modifyRegBits(VEML7700_ALS_POWER_SAVE, 0x0006, (mode << 1));
}

uint8_t Adafruit_VEML7700::getPowerSaveMode()
{
    return (readReg16(VEML7700_ALS_POWER_SAVE) >> 1) & 0x03;
}

// -------------------- Thresholds --------------------

void Adafruit_VEML7700::setLowThreshold(uint16_t v)
{
    writeReg16(VEML7700_ALS_THREHOLD_LOW, v);
}

uint16_t Adafruit_VEML7700::getLowThreshold()
{
    return readReg16(VEML7700_ALS_THREHOLD_LOW);
}

void Adafruit_VEML7700::setHighThreshold(uint16_t v)
{
    writeReg16(VEML7700_ALS_THREHOLD_HIGH, v);
}

uint16_t Adafruit_VEML7700::getHighThreshold()
{
    return readReg16(VEML7700_ALS_THREHOLD_HIGH);
}

uint16_t Adafruit_VEML7700::interruptStatus()
{
    return readReg16(VEML7700_INTERRUPTSTATUS);
}

// -------------------- ALS Reading --------------------

void Adafruit_VEML7700::readWait()
{
    uint64_t now = esp_timer_get_time()/1000;
    int toWait = 2 * getIntegrationTimeValue();
    int waited = now - lastRead;

    if (waited < toWait)
        vTaskDelay(pdMS_TO_TICKS(toWait - waited));
}

uint16_t Adafruit_VEML7700::readALS(bool wait)
{
    if (wait) readWait();
    lastRead = esp_timer_get_time()/1000;
    return readReg16(VEML7700_ALS_DATA);
}

uint16_t Adafruit_VEML7700::readWhite(bool wait)
{
    if (wait) readWait();
    lastRead = esp_timer_get_time()/1000;
    return readReg16(VEML7700_WHITE_DATA);
}

// -------------------- Lux Calculation --------------------

float Adafruit_VEML7700::getResolution()
{
    return MAX_RES * (IT_MAX / getIntegrationTimeValue()) *
           (GAIN_MAX / getGainValue());
}

float Adafruit_VEML7700::computeLux(uint16_t raw, bool corrected)
{
    float lux = getResolution() * raw;
    if (corrected) {
        lux = (((6.0135e-13 * lux - 9.3924e-9) * lux + 8.1488e-5) * lux + 1.0023) * lux;
    }
    return lux;
}

float Adafruit_VEML7700::readLux(luxMethod method)
{
    bool wait = true;

    switch (method) {
        case VEML_LUX_NORMAL_NOWAIT: wait = false;
        case VEML_LUX_NORMAL: return computeLux(readALS(wait));

        case VEML_LUX_CORRECTED_NOWAIT: wait = false;
        case VEML_LUX_CORRECTED: return computeLux(readALS(wait), true);

        case VEML_LUX_AUTO: return autoLux();
    }
    return -1;
}

// -------------------- Auto Lux --------------------

float Adafruit_VEML7700::autoLux()
{
    const uint8_t gains[] = {VEML7700_GAIN_1_8, VEML7700_GAIN_1_4,
                             VEML7700_GAIN_1, VEML7700_GAIN_2};

    const uint8_t itTimes[] = {VEML7700_IT_25MS, VEML7700_IT_50MS,
                               VEML7700_IT_100MS, VEML7700_IT_200MS,
                               VEML7700_IT_400MS, VEML7700_IT_800MS};

    uint8_t gi = 0;
    uint8_t ii = 2;
    bool corrected = false;

    setGain(gains[gi]);
    setIntegrationTime(itTimes[ii]);

    uint16_t als = readALS(true);

    if (als <= 100) {
        while (als <= 100 && !(gi == 3 && ii == 5)) {
            if (gi < 3) setGain(gains[++gi]);
            else if (ii < 5) setIntegrationTime(itTimes[++ii]);
            als = readALS(true);
        }
    } else {
        corrected = true;
        while (als > 10000 && ii > 0) {
            setIntegrationTime(itTimes[--ii]);
            als = readALS(true);
        }
    }

    return computeLux(als, corrected);
}

#ifdef __cplusplus
}
#endif
