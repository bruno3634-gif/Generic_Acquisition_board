/******************************/
/****       -Defines-      ****/
/******************************/

#define MPU6050 0
#define ADXL345 1
#define A01NYUB 1
#define SEN07024 0


#define READINGS_NUM 6
/*******************************/
/****       -Includes-      ****/
/*******************************/

#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Wire.h>
#include <SoftwareSerial.h>

#if ADXL345
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#endif
#if MPU6050
#include <Adafruit_MPU6050.h>
#endif

/*******************************************/
/****       -Function defenitions-      ****/
/*******************************************/

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
void IRAM_ATTR onTimer();
void init_sensor();
void read_sensor();
void ultrassonic_sensor();


/********************************/
/****       -Variables-      ****/
/********************************/

uint8_t requesterMAC[6];
hw_timer_t *timer = NULL;
#define TIMER_INTERVAL_US 150000
unsigned char data[4];
int distance = 0;

typedef struct struct_message
{
  float ax;
  float ay;
  float az;
  int distance;
  int tempo;
} struct_message;

/********************************/
/****       -Functions-      ****/
/********************************/

SoftwareSerial mySerial(4, 5);
#if mpu6050
Adafruit_MPU6050 mpu;
#endif
#if ADXL345
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
#endif
#if SEN07022
Adafruit_SGP30 sgp;
#endif


void setup()
{
  Serial.begin(115200);

  Serial.println("Starting setup...");
  WiFi.mode(WIFI_STA);
  
  Serial.println("WiFi Mode set to STA.");

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  Serial.println("ESP-NOW initialized");

  // Register the callback to receive data
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("ESP-NOW started and callback registered");

  timer = timerBegin(0, 80, true);                 // Timer 0, prescaler of 80 (1 tick = 1 µs)
  timerAttachInterrupt(timer, &onTimer, true);     // Attach ISR to the timer
  timerAlarmWrite(timer, TIMER_INTERVAL_US, true); // Set alarm interval to 150 ms
}

void loop()
{
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
  Serial.println("Received signal to send data");
  memcpy(requesterMAC, mac_addr, 6); // Store the requester's MAC address
  timerAlarmEnable(timer);           // Enable the timer alarm
}

void IRAM_ATTR onTimer()
{
  for (int i = 0; i < READINGS_NUM; i++)
  {
    read_sensor();
  }
  timerAlarmDisable(timer); // Disable the timer alarm
  esp_err_t result = esp_now_send(requesterMAC, (uint8_t *)&sensor_data, sizeof(sensor_data));
}

void init_sensor()
{
#if MPU6050
  if (!mpu.begin())
  {
    Serial.println("Failed to find MPU6050 chip");
    while (1)
    {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");
#endif
#if ADXL345
  if (!accel.begin())
  {
    Serial.println("Failed to find ADXL345 chip");
    while (1)
    {
      delay(10);
    }
  }
  Serial.println("ADXL345 Found!");
#endif
}

void read_sensor()
{ // store sensor data in struct_message
  struct_message sensor_data;
#if MPU6050
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  sensor_data.ax = a.acceleration.x;
  sensor_data.ay = a.acceleration.y;
  sensor_data.az = a.acceleration.z;
#endif
#if ADXL345
  sensors_event_t event;
  accel.getEvent(&event);
  sensor_data.ax = event.acceleration.x;
  sensor_data.ay = event.acceleration.y;
  sensor_data.az = event.acceleration.z;
#endif
  sensor_data.distance = 0; // distance sensor data
  sensor_data.tempo = 0;    // tempo sensor data
#if A01NYUB || SEN07024
  ultrassonic_sensor();
#endif
}

void ultrassonic_sensor()
{
  if (mySerial.available() >= 4)
  {
    for (int i = 0; i < 4; i++)
    {
      data[i] = mySerial.read();
    }

    // Check if the first byte is 0xff
    if (data[0] == 0xff)
    {
      int sum = (data[0] + data[1] + data[2]) & 0x00FF;
      if (sum == data[3])
      {
        distance = (data[1] << 8) + data[2];
        if (distance > 30)
        {
          // Print the distance in millimeters
          Serial.print("distance=");
          Serial.print(distance);
          Serial.println("mm");
          //sensor_data.distance = distance;
        }
        else
        {
          Serial.println("Below the lower limit");
        }
      }
      else
      {
        Serial.println("ERROR: Checksum mismatch");
      }
    }
    else
    {
      Serial.println("ERROR: Invalid start byte");
    }
  }
  else
  {
    Serial.println("ERROR: Not enough data available");
  }
}