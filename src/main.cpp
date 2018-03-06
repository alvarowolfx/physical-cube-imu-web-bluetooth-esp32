
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#include <Wire.h>
#include <MadgwickAHRS.h>
#include <WS2812FX.h>

#define SAMPLE_RATE 20
#define REPORTING_PERIOD_MS 20
#define DEBUG false
#define IMU_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define ACC_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26b8"
#define GYRO_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define ANGLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26c8"

#define COLOR_SERVICE_UUID "5051269d-7791-4332-b534-c6caab0ae14b"
#define COLOR_CHARACTERISTIC_UUID "3bac26a0-e902-44d5-8820-d8698543147b"

#define LED_PIN 18
#define NUMPIXELS 16
#define FX_SPEED 220
#define FX_BRIGHTNESS 100

WS2812FX ws2812fx = WS2812FX(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

class ColorCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        if (value.length() == 3)
        {
            int red = value[0];
            int green = value[1];
            int blue = value[2];

            if (DEBUG)
            {
                Serial.println("Valid");
                Serial.print("Value ");
                Serial.print(red);
                Serial.print(", ");
                Serial.print(green);
                Serial.print(", ");
                Serial.println(blue);
            }

            ws2812fx.setColor(red, green, blue);
        }
    }
};

BLEServer *pServer = NULL;
BLEService *imuService = NULL;
BLECharacteristic *accelerometerDataCharacteristic = NULL;
BLECharacteristic *gyroscopeDataCharacteristic = NULL;
BLECharacteristic *angleDataCharacteristic = NULL;
BLEService *colorService = NULL;
BLECharacteristic *colorDataCharacteristic = NULL;
BLEAdvertising *pAdvertising = NULL;

//MPU6050 I2C Address
#define MPU 0x68
#define PWR_MGMT_1 0x6B
#define GYRO_CONFIG 0x1B
#define ACCEL_CONFIG 0x1C
#define ACCEL_XOUT 0x3B

//Sensors Data
#define SAMPLE_SIZE 30
int16_t AcX[SAMPLE_SIZE], AcY[SAMPLE_SIZE], AcZ[SAMPLE_SIZE], Tmp[SAMPLE_SIZE], GyX[SAMPLE_SIZE], GyY[SAMPLE_SIZE], GyZ[SAMPLE_SIZE];
int currentSampleIndex = 0;
Madgwick filter;

const int ACC_MODE = 0;
const int GYRO_MODE = 0;
float gyroScales[4] = {131.0f, 65.5f, 32.8f, 16.4f};
float accScales[4] = {16384.0f, 8192.0f, 4096.0f, 2048.0f};
/* SCALING FACTORS 

Angular Velocity Limit  |   Sensitivity
----------------------------------------
250º/s                  |    131
500º/s                  |    65.5 
1000º/s                 |    32.8 
2000º/s                 |    16.4

Acceleration Limit  |   Sensitivity
----------------------------------------
2g                  |    16,384
4g                  |    8,192  
8g                  |    4,096 
16g                 |    2,048 
*/

long lastReport = 0;
long lastSample = 0;

typedef union {
    float value;
    byte bytes[4];
} Reading;

void writeRegMPU(int reg, int val)
{
    Wire.beginTransmission(MPU);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission(false);
}

void setupMPU()
{
    Wire.begin(21, 22);
    writeRegMPU(PWR_MGMT_1, 0);
    writeRegMPU(GYRO_CONFIG, GYRO_MODE); // Gyro config
    writeRegMPU(ACCEL_CONFIG, ACC_MODE); // Acc config
}

void readMPU()
{
    Wire.beginTransmission(MPU);
    Wire.write(ACCEL_XOUT); // starting with register 0x3B (ACCEL_XOUT_H)
    Wire.endTransmission(false);
    // Request sensor data
    Wire.requestFrom(MPU, 14);
    // Store sensors values
    AcX[currentSampleIndex] = Wire.read() << 8 | Wire.read(); //0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
    AcY[currentSampleIndex] = Wire.read() << 8 | Wire.read(); //0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
    AcZ[currentSampleIndex] = Wire.read() << 8 | Wire.read(); //0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
    Tmp[currentSampleIndex] = Wire.read() << 8 | Wire.read(); //0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
    GyX[currentSampleIndex] = Wire.read() << 8 | Wire.read(); //0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
    GyY[currentSampleIndex] = Wire.read() << 8 | Wire.read(); //0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
    GyZ[currentSampleIndex] = Wire.read() << 8 | Wire.read(); //0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
    currentSampleIndex++;
    if (currentSampleIndex >= SAMPLE_SIZE)
    {
        currentSampleIndex = 0;
    }
}

float avgSample(int16_t values[SAMPLE_SIZE])
{
    float value = 0;
    for (int i = 0; i < SAMPLE_SIZE; i++)
    {
        value += values[i] * 1.0f;
    }
    return (value / (1.0f * SAMPLE_SIZE));
}

void setup()
{
    Serial.begin(9600);

    setupMPU();
    filter.begin(SAMPLE_RATE);

    ws2812fx.init();
    ws2812fx.setBrightness(FX_BRIGHTNESS);
    ws2812fx.setSpeed(FX_SPEED);
    ws2812fx.setColor(0x00FF00);
    ws2812fx.setMode(FX_MODE_BREATH);
    ws2812fx.start();

    BLEDevice::init("AwesomeMinecraftBlock");

    pServer = BLEDevice::createServer();

    imuService = pServer->createService(IMU_SERVICE_UUID);
    accelerometerDataCharacteristic = imuService->createCharacteristic(
        ACC_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    gyroscopeDataCharacteristic = imuService->createCharacteristic(
        GYRO_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    angleDataCharacteristic = imuService->createCharacteristic(
        ANGLE_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    imuService->start();

    colorService = pServer->createService(COLOR_SERVICE_UUID);
    colorDataCharacteristic = colorService->createCharacteristic(
        COLOR_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    colorDataCharacteristic->setCallbacks(new ColorCallbacks());
    colorService->start();

    pAdvertising = pServer->getAdvertising();
    pAdvertising->start();

    for (int i = 0; i < SAMPLE_SIZE; i++)
    {
        readMPU();
    }
}

void loop()
{
    // Make sure to call update as fast as possible
    ws2812fx.service();
    readMPU();

    long dT = millis() - lastSample;
    if (dT > SAMPLE_RATE)
    {

        float ax = avgSample(AcX) / accScales[ACC_MODE] + 0.08f;
        float ay = avgSample(AcY) / accScales[ACC_MODE] - 0.04f;
        float az = avgSample(AcZ) / accScales[ACC_MODE] + 0.07f;

        float gx = avgSample(GyX) / gyroScales[GYRO_MODE];
        float gy = avgSample(GyY) / gyroScales[GYRO_MODE];
        float gz = avgSample(GyZ) / gyroScales[GYRO_MODE];

        float gyroScale = 0.5f;

        filter.updateIMU(gx * gyroScale, gy * gyroScale, gz * gyroScale, ax, ay, az);
        lastSample = millis();
    }

    dT = millis() - lastReport;

    if (dT > REPORTING_PERIOD_MS)
    {
        float ax = avgSample(AcX) / accScales[ACC_MODE] + 0.08f;
        float ay = avgSample(AcY) / accScales[ACC_MODE] - 0.04f;
        float az = avgSample(AcZ) / accScales[ACC_MODE] + 0.07f;

        float gx = avgSample(GyX) / gyroScales[GYRO_MODE];
        float gy = avgSample(GyY) / gyroScales[GYRO_MODE];
        float gz = avgSample(GyZ) / gyroScales[GYRO_MODE];

        float roll = filter.getRoll();
        float pitch = filter.getPitch();
        float heading = filter.getYaw();

        if (DEBUG)
        {
            Serial.print("AcX:");
            Serial.print(ax);
            Serial.print(", AcY:");
            Serial.print(ay);
            Serial.print(", AcZ:");
            Serial.println(az);
            //Serial.print("Tmp:");
            //Serial.println(Tmp);
            Serial.print("GyX:");
            Serial.print(gx);
            Serial.print(", GyY:");
            Serial.print(gy);
            Serial.print(", GyZ:");
            Serial.println(gz);

            Serial.print("Orientation: ");
            Serial.print(heading);
            Serial.print(" ");
            Serial.print(pitch);
            Serial.print(" ");
            Serial.println(roll);
        }

        // Error if all zero
        if (ax == 0.0f && ay == 0.0f && az == 0.0f)
        {
            ws2812fx.setColor(0xFF0000);
            ws2812fx.setMode(FX_MODE_STATIC);
        }

        Reading axReading, ayReading, azReading;
        axReading.value = ax;
        ayReading.value = ay;
        azReading.value = az;

        Reading gxReading, gyReading, gzReading;
        gxReading.value = gx;
        gyReading.value = gy;
        gzReading.value = gz;

        Reading agxReading, agyReading, agzReading;
        agxReading.value = roll;
        agyReading.value = pitch;
        agzReading.value = heading;

        uint8_t accData[12] = {
            (uint8_t)axReading.bytes[0],
            (uint8_t)axReading.bytes[1],
            (uint8_t)axReading.bytes[2],
            (uint8_t)axReading.bytes[3],
            (uint8_t)ayReading.bytes[0],
            (uint8_t)ayReading.bytes[1],
            (uint8_t)ayReading.bytes[2],
            (uint8_t)ayReading.bytes[3],
            (uint8_t)azReading.bytes[0],
            (uint8_t)azReading.bytes[1],
            (uint8_t)azReading.bytes[2],
            (uint8_t)azReading.bytes[3]};

        uint8_t gyroData[12] = {
            (uint8_t)gxReading.bytes[0],
            (uint8_t)gxReading.bytes[1],
            (uint8_t)gxReading.bytes[2],
            (uint8_t)gxReading.bytes[3],
            (uint8_t)gyReading.bytes[0],
            (uint8_t)gyReading.bytes[1],
            (uint8_t)gyReading.bytes[2],
            (uint8_t)gyReading.bytes[3],
            (uint8_t)gzReading.bytes[0],
            (uint8_t)gzReading.bytes[1],
            (uint8_t)gzReading.bytes[2],
            (uint8_t)gzReading.bytes[3]};

        uint8_t angleData[12] = {
            (uint8_t)agxReading.bytes[0],
            (uint8_t)agxReading.bytes[1],
            (uint8_t)agxReading.bytes[2],
            (uint8_t)agxReading.bytes[3],
            (uint8_t)agyReading.bytes[0],
            (uint8_t)agyReading.bytes[1],
            (uint8_t)agyReading.bytes[2],
            (uint8_t)agyReading.bytes[3],
            (uint8_t)agzReading.bytes[0],
            (uint8_t)agzReading.bytes[1],
            (uint8_t)agzReading.bytes[2],
            (uint8_t)agzReading.bytes[3]};

        accelerometerDataCharacteristic->setValue(accData, 12);
        //accelerometerDataCharacteristic->notify();

        gyroscopeDataCharacteristic->setValue(gyroData, 12);
        //gyroscopeDataCharacteristic->notify();

        angleDataCharacteristic->setValue(angleData, 12);
        //angleDataCharacteristic->notify();

        lastReport = millis();
    }
}