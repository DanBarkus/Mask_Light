#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <RunningAverage.h>
#include <Adafruit_NeoPixel.h>

Adafruit_BMP280 bmp; // I2C

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1:
#define LED_PIN 6

// How many NeoPixels are attached to the Arduino?
#define LED_COUNT 89

// NeoPixel brightness, 0 (min) to 255 (max)
#define BRIGHTNESS 50

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

// Pressure readings averaging params
const int numReadings = 100;
float readings[numReadings]; // the readings from the analog input
int readIndex = 0;           // the index of the current reading
float total = 0;             // the running total
float average = 0;           // the average

const int numSlopeReadings = 256;
float slopes[numSlopeReadings];
int slopeIndex = 0;
long slopeTotal = 0.0;
float slopeAverage = 0.0;

float threshold = 15.0;

float scoreScalar = 0.5;

float breathTimeout = 2 * 1000;

// 0 - not breathing
// 1 - inhaling
// 2 - exhaling
int mode = 0;

float pressure = 0.0;
float lastPressure = 0.0;
float startPressure = 0.0;
float currTime = 0.0;
float lastTime = 0.0;
float startTime = 0.0;

float slope = 0.0;

float distribution[4][10] = {{0.1, 0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                             {0.1, 0.1, 0.2, 0.3, 0.4, 0.4, 0.2, 0.2, 0.1, 0.0},
                             {0.5, 0.5, 0.7, 0.7, 0.9, 0.9, 0.8, 0.5, 0.2, 0.1},
                             {0.8, 0.8, 0.9, 0.9, 1.0, 1.0, 1.0, 0.8, 0.5, 0.3}};

void setup()
{
    Serial.begin(9600);
    Serial.println(F("BMP280 test"));

    if (!bmp.begin())
    {
        Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
        while (1)
            ;
    }

    /* Default settings from datasheet. */
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,   /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X1,   /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,  /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,    /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_1); /* Standby time. */

    strip.begin();            // INITIALIZE NeoPixel strip object (REQUIRED)
    strip.show();             // Turn OFF all pixels ASAP
    strip.setBrightness(100); // Set BRIGHTNESS to about 1/5 (max = 255)
    for (int i = 0; i < numSlopeReadings; i++) {
        slopes[i] = 0.0;
    }

    delay(1000);
}

void loop()
{
    // Serial.println(mode);
    lastPressure = pressure;
    pressure = getNextReading();
    slope = getSlope(pressure);

    lastTime = currTime;
    currTime = millis();
    // Serial.print(pressure);
    // Serial.print(",");
    // Serial.println(pressure);

    // currently not taking a breath
    if (mode == 0)
    {
        if (slope > threshold)
        {
            startPressure = pressure - threshold;
            startTime = currTime;
            mode = 2;
        }
        else if (slope < threshold * -1.0)
        {
            startPressure = pressure + threshold;
            startTime = currTime;
            mode = 1;
        }
        strip.fill(strip.Color(0, 0, 0, 0));
    }
    else
    {
        float score = getScore(startPressure, pressure, slope);
        // Serial.println(score);
        drawLeft(score, mode);
        drawRight(score, mode);

    }
    strip.show();
}

float getNextReading()
{
    total = total - readings[readIndex];
    readings[readIndex] = bmp.readPressure();
    total = total + readings[readIndex];
    readIndex++;
    average = total / numReadings;

    if (readIndex >= numReadings)
    {
        readIndex = 0;
    }

    return average;
}

float getSlope(float pressure)
{
    // y = mX + b
    // m = (y-b)/x
    slopeTotal = slopeTotal - slopes[slopeIndex];
    float slp = ((pressure - lastPressure) / (currTime - lastTime)) * 1000.0;
    slp = constrain(slp, -10000.0, 10000.0);
    slopes[slopeIndex] = slp;
    slopeTotal = slopeTotal + slopes[slopeIndex];
    slopeIndex++;

    if (slopeIndex >= numSlopeReadings)
    {
        slopeIndex = 0;
    }

    slopeAverage = slopeTotal / numSlopeReadings;
    return slopeAverage;
}

float getScore(float startPressure, float pressure, float slope)
{
    float score = 0;
    if ((pressure < startPressure && mode == 2) || 
        (pressure > startPressure && mode == 1) ||
        (currTime - breathTimeout > startTime)) {
        Serial.println("Timeout");
        mode = 0;
    }
    else {
        score = abs(startPressure - pressure) * scoreScalar;
        Serial.println(score);
    }

    return score;
}

void setStripColor(float diff, bool in)
{
    diff = abs(diff) - threshold;
    uint32_t color = 0;
    if (in)
    {
        color = strip.Color(0, 0, diff * 2.0, 6);
    }
    else
    {
        color = strip.Color(diff * 2.0, 0, 0, 6);
    }
    strip.fill(color);
    // Serial.println(diff*50.0);
}

void drawLeft(float diff, int in)
{
    uint32_t color = 0;

    for (int i = 0; i < 40; i++)
    {
        int indexY = i % 10;
        int indexX = i / 10;
        if (in == 1)
        {
            color = strip.Color(0, 0, diff * distribution[indexX][indexY], 0);
        }
        else if (in == 2)
        {
            color = strip.Color(diff * distribution[indexX][indexY], 0, 0, 0);
        }
        else {
            color = strip.Color(0, 0, 0, 0);
        }

        strip.setPixelColor(i, color);
    }
}

void drawRight(float diff, int in)
{
    uint32_t color = 0;

    for (int i = 88; i > 48; i--)
    {
        int indexY = (i - 49) % 10;
        int indexX = 3 - ((i - 49) / 10);
        if (in == 1)
        {
            color = strip.Color(0, 0, diff * distribution[indexX][indexY], 0);
        }
        else if (in == 2)
        {
            color = strip.Color(diff * distribution[indexX][indexY], 0, 0, 0);
        }
        else {
            color = strip.Color(0, 0, 0, 0);
        }


        strip.setPixelColor(i, color);
    }
}
