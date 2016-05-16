#include <Arduino.h>
// Include the SD library.
#include <SPI.h>
#include <SD.h>
#include <Servo.h>

// Include sensor libraries.
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <Adafruit_ADXL345_U.h>
// #include <Adafruit_HMC5883_U.h>

#define SERIAL_DEBUG

// Variables for Sensors.
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(1);
Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(2);
// Adafruit_HMC5883_Unified hmc = Adafruit_HMC5883_Unified(3);

File logFile;
Servo parachute;

float groundLevel;
unsigned int status = 0;

struct full_event {
    sensors_vec_t acceleration;
    float accel_total;
    // sensors_vec_t gyro; // The gyro is dead
    // sensors_vec_t magnetic;
    float agl; // based off of pressure.
};

// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
const unsigned int chipSelect = 10;
const float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;

bool trigger_parachute(struct full_event *event) {
    return (event->accel_total < 0.2);
}

bool trigger_launch(struct full_event *event) {
    return (event->agl > 3.0);
}

void data_print(String prefix, double value, String postfix, bool nl = false) {
    #ifdef SERIAL_DEBUG
    Serial.print(prefix+" "); Serial.print(value, DEC); Serial.println(" "+postfix);
    #endif
    if (!nl) {
      logFile.print(value); logFile.print(',');
    }
    else logFile.println(value);
}

void setup() {
    // Open serial communications and wait for port to open:
    parachute.attach(9);
    parachute.write(15);
    #ifdef SERIAL_DEBUG
    Serial.begin(9600);
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB port only
    }
    #endif

    #ifdef SERIAL_DEBUG
    if (!SD.begin(chipSelect)) Serial.println("Something went wrong w/ SD!");
    if (!bmp.begin()) Serial.println("Something went wrong w/ BMP!");
    if (!adxl.begin()) Serial.println("Something went wrong w/ ADXL!");
    //  if (!hmc.begin()) Serial.println("Something went wrong w/ HMC!");
    #else
    SD.begin(chipSelect);
    bmp.begin();
    adxl.begin();
    // hmc.begin();
    #endif

    adxl.setRange(ADXL345_RANGE_16_G);

    { // Increment log file number.
        unsigned long logNum = 0;
        File numFile;
        if (SD.exists(const_cast<char*>("num"))) {
            numFile = SD.open("num", FILE_READ);
            logNum = numFile.parseInt();
            numFile.close();
            SD.remove(const_cast<char*>("num"));
        }
        ++logNum;
        numFile = SD.open("num", FILE_WRITE);
        numFile.print(logNum);
        numFile.close();
        String fileName = String(logNum);
        fileName.concat(".csv");
        logFile = SD.open(fileName.c_str(), FILE_WRITE);
        logFile.println("Millis,AGL (FT),Total Gs,Status");
    }
    {
        sensors_event_t altitude;
        bmp.getEvent(&altitude);
        groundLevel = bmp.pressureToAltitude(seaLevelPressure, altitude.pressure) * 3.2808;
    }
}


void loop(void) {
    sensors_event_t event;
    struct full_event data;

    logFile.print(millis()); logFile.print(',');
    // hmc.getEvent(&event);
    // data.magnetic = event.magnetic;
    // data_print("Mag X:", data.magnetic.x, "uT");
    // data_print("Mag Y:", data.magnetic.y, "uT");
    // data_print("Mag Z:", data.magnetic.z, "uT");

    bmp.getEvent(&event);
    // data_print("Pressure:", event.pressure, "hPa"); // No need to log pressure.
    // {
    //   float temperature; // Is there really any reason to calculate temperature?
    //   bmp.getTemperature(&temperature);
    //   data_print("Temperature:", temperature, "*C");
    // }
    data.agl = (bmp.pressureToAltitude(seaLevelPressure, event.pressure) * 3.2808) - groundLevel;
    data_print("AGL:", data.agl, "ft");
    // Serial.println("");

    adxl.getEvent(&event);
    // data.acceleration = event.acceleration;
    // data_print("Accel X:", data.acceleration.x, "m/s^2");
    // data_print("Accel Y:", data.acceleration.y, "m/s^2");
    // data_print("Accel Z:", data.acceleration.z, "m/s^2");
    {
        data.accel_total = (sqrt(sq(event.acceleration.x) +
                                 sq(event.acceleration.y) +
                                 sq(event.acceleration.z)) - 0.8) / 9.80665;

        data_print("Accel Total:", data.accel_total, "G");
    }

    if (trigger_launch(&data) && !status) {
        status = 1;
    }

    if (trigger_parachute(&data) && (status < 10)) {
      parachute.write(180); // Open parachute.
      status = 10;
    }

    logFile.println(status);
    logFile.flush();
}
