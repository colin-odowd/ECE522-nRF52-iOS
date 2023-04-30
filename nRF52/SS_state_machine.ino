#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <Wire.h>
#include "MAX30105.h"
#include <spo2_algorithm.h>
#include "Adafruit_SHT31.h"
#include <Adafruit_BMP280.h>
#include <Adafruit_LSM6DS33.h>
#include <Arduino.h>

/* References */
/* Heart Rate and Oxygen Saturation Algorithms */
/* https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library */

#define FOREVER 1
#define MAX_BRIGHTNESS 255

// BLE Service Globals
BLEDfu  bledfu;  // OTA DFU service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble
BLEBas  blebas;  // battery
bool ble_connected = false;
char ble_buffer[64];

// Device State Globals
enum device_states {
  initialization,
  standby,
  monitoring,
  alarm,
  maintenance
};

int device_state = initialization;

// Sensor Hardware Globals
MAX30105 particleSensor;
Adafruit_SHT31 sht31 = Adafruit_SHT31();  
Adafruit_BMP280 bmp280;  
Adafruit_LSM6DS33 accelerometer;

// MAX30105 Variables
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data
int32_t bufferLength = 0; //data length
int32_t spo2 = 0; //SPO2 value
int8_t validSPO2 = 0; //indicator to show if the SPO2 calculation is valid
int32_t heartRate = 0; //heart rate value
int8_t validHeartRate = 0; //indicator to show if the heart rate calculation is valid

byte pulseLED = 11; //Must be on PWM pin
byte readLED = 13; //Blinks with each data read

//******************************************************************************/
// beat rate minute -> accurate
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
int beatAvg = 0;
int heart_rate_output_orig = 0;
int heart_rate_output = 0;
int heat_rate_sum = 0;
int hr_cal_cnt = 0;
/******************************************************************************/

/******************************************************************************/
//spo2 rate minute -> accurate
const byte spo2_RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte spo2_rates[spo2_RATE_SIZE]; //Array of heart rates
byte spo2_rateSpot = 0;
int spo2_Avg = 0;

//spo2 rate minute -> detect
const byte spo2_RATE_SIZE_d = 4; //Increase this for more averaging. 4 is good.
byte spo2_rates_d[spo2_RATE_SIZE_d]; //Array of heart rates
byte spo2_rateSpot_d = 0;
int spo2_Avg_d = 0;

// spo2 value controller
int fail_cnt=0;
int8_t accurate_val_valid = 0;
int ouput_spo2_val = 0;
int spo2_val_accurate_cnt = 0;
int validSPO2_cnt = 0;
/******************************************************************************/
//temperature and humidity
int temperature;  
int humidity;

/******************************************************************************/
// breathing rate global variables
//Variables:
int breathing_rate_value; //save analog value
int flexPin = 19;
bool inhale = false;
bool tenSecMark = false;
unsigned long millis();
float count = 0;
int breathingRate;
int timeCheck = 0;
/******************************************************************************/

/******************************************************************************/
//accelerometer globals
int ax, ay, az;
sensors_event_t accel, gyro, temp;

/******************************************************************************/

void setup() {
  Serial.begin(115200);
  bluetooth_init();
  spo2_init();
  temperature_and_humidity_init();
  accelerometer_init();
}

void loop() {
  while (FOREVER)
  {
    switch (device_state)
    {
      case initialization:
        //Waiting on BLE callback function
        delay(250);
        Serial.print("Init");
      case monitoring:
        Serial.print("Mon");
        spo2_read();
        breathing_rate_read();
        temperature_read();
        humidity_read();
        accelerometer_read();
        send_to_BLE();
        delay(100);
        break;
      case alarm:
        break;
      default:
        break;
    }
  }
}

void bluetooth_init() {

  // Config the peripheral connection with maximum bandwidth 
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  //Bluefruit.setName(getMcuUniqueID()); // useful testing with multiple central connections
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();

  // Configure and Start BLE Uart Service
  bleuart.begin();

  // Start BLE Battery Service
  blebas.begin();
  blebas.write(100);

  // Set up and start advertising
  startAdv();
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void connect_callback(uint16_t conn_handle)
{
  device_state = monitoring;
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  device_state = initialization;
}

void spo2_init()
{
  pinMode(pulseLED, OUTPUT);
  pinMode(readLED, OUTPUT);

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    while (1);
  }

  byte ledBrightness = 40; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

  bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps

  //read the first 100 samples, and determine the signal range
  for (byte i = 0 ; i < bufferLength ; i++)
  {
    while (particleSensor.available() == false) //do we have new data?
      particleSensor.check(); //Check the sensor for new data

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample(); //We're finished with this sample so move to next sample
  }

  //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

}

void temperature_and_humidity_init() {
  bmp280.begin();
  while (!Serial) delay(10);
  if ( !sht31.begin(0x44) ) {
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }
}

void accelerometer_init() {
  accelerometer.begin_I2C();
}

void spo2_read() {
  //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
  for (byte i = 25; i < 100; i++)
  {
    redBuffer[i - 25] = redBuffer[i];
    irBuffer[i - 25] = irBuffer[i];
  }

  //take 25 sets of samples before calculating the heart rate.
  for (byte i = 75; i < 100; i++)
  {
    while (particleSensor.available() == false) //do we have new data?
      particleSensor.check(); //Check the sensor for new data

    digitalWrite(readLED, !digitalRead(readLED)); //Blink onboard LED with every data read

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample(); //We're finished with this sample so move to next sample

    //***********HEART RATE******************//
    //heart rate per minute: try not be so sensitive 

    //accurate
    if (heartRate < 200 && heartRate > 40)
    {
      rates[rateSpot++] = (byte)heartRate; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable

      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
        //beatAvg /= RATE_SIZE;
    }

    heat_rate_sum = heat_rate_sum + (beatAvg/10)  ;//heat_rate_sum= 0 at first
    hr_cal_cnt++;
    if (hr_cal_cnt >=20)
    {
      heart_rate_output_orig = heat_rate_sum/20;
      heart_rate_output = heart_rate_output_orig;
      hr_cal_cnt=0;
      heat_rate_sum = 0;
    }

    if(validHeartRate==0)heart_rate_output=0;;
    //****************************************//

    //***********SPO2******************//
    //SPO2 per minute: try not be so sensetive ->"Accurate Version"
    if (spo2 <101 && spo2 > 94)
    {
      spo2_val_accurate_cnt ++;
      spo2_rates[spo2_rateSpot++] = (byte)spo2; //Store this reading in the array
      spo2_rateSpot %= spo2_RATE_SIZE; //Wrap variable

      //Take average of readings
      spo2_Avg = 0;
      for (byte y = 0 ; y < spo2_RATE_SIZE ; y++)
        spo2_Avg += spo2_rates[y];
        spo2_Avg /= spo2_RATE_SIZE;
    }
    //SPO2 per minute: try not be so sensetive ->"Detect Version"
    if (spo2 <101 && spo2 > 40)
    {
      spo2_rates_d[spo2_rateSpot_d++] = (byte)spo2; //Store this reading in the array
      spo2_rateSpot_d %= spo2_RATE_SIZE_d; //Wrap variable

      //Take average of readings
      spo2_Avg_d = 0;
      for (byte z = 0 ; z < spo2_RATE_SIZE_d ; z++)
        spo2_Avg_d += spo2_rates_d[z];
      spo2_Avg_d /= spo2_RATE_SIZE_d;
    }
    else if (spo2=-999) spo2 = 0;
    //****************************************//
    //Output Value Controler
    if(validSPO2) 
    {
      validSPO2_cnt++;
    }

    if (validSPO2_cnt>50) 
    {
      if (spo2_Avg > 94) {
        accurate_val_valid =1;
        fail_cnt =0;
      }
      else fail_cnt ++;
      spo2_val_accurate_cnt=0;
      validSPO2_cnt=0;
    }

    //****************************************//

    if(fail_cnt <5){    
      ouput_spo2_val =  spo2_Avg;   
    }
    else{
      ouput_spo2_val =  spo2_Avg_d;
    }      

    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
  }
}

void breathing_rate_read() {

  breathing_rate_value = analogRead(flexPin);
  
  if (breathing_rate_value >= 530 && inhale == false) {
    count++;
    inhale = true;
  } else if (breathing_rate_value < 530) {
    inhale = false;
  }

  timeCheck = (millis() % 10000) / 1000;
  if ((timeCheck == 0) && (tenSecMark == false)) {
    breathingRate = (count / 10) * 60;
    count = 0;
    tenSecMark = true;   
  } else if (timeCheck != 0) {
    tenSecMark = false;
  }

  Serial.println("");
  Serial.print("BR Value: ");
  Serial.println(breathing_rate_value);
  Serial.print("Count: ");
  Serial.println(count);
  Serial.print("BR: ");
  Serial.println(breathingRate);
  delay(100);

}

void temperature_read() {
  temperature = bmp280.readTemperature(); 
}

void humidity_read() {
  humidity = sht31.readHumidity();
}

void accelerometer_read() {

  accelerometer.getEvent(&accel, &gyro, &temp);

  ax = int(accel.acceleration.x*100)%100;
  ay = int(accel.acceleration.y*100)%100;
  az = int(accel.acceleration.z*100)%100;

  delay(25);
}


void send_to_BLE() {

  String value;
  Serial.println("Sending to BLE");

  if (validHeartRate) {
    value = String(heart_rate_output);
    value = "HRM: " + value;
    value.toCharArray(ble_buffer, 64);
    bleuart.write(ble_buffer, strlen(ble_buffer)); 
  } else {
    value = String(validHeartRate);
    value = "HRM: " + value;
    value.toCharArray(ble_buffer, 64);
    bleuart.write(ble_buffer, strlen(ble_buffer)); 
  }
  delay(25);
  
  if (validSPO2) {
    value = String(ouput_spo2_val);
    value = "OxySat: " + value;
    value.toCharArray(ble_buffer, 64);
    bleuart.write(ble_buffer, strlen(ble_buffer)); 
  } else {
    value = String(validSPO2);
    value = "OxySat: " + value;
    value.toCharArray(ble_buffer, 64);
    bleuart.write(ble_buffer, strlen(ble_buffer)); 
  }
  delay(25);

  value = String(breathingRate);
  value = "BR: " + value;
  value.toCharArray(ble_buffer, 64);
  bleuart.write(ble_buffer, strlen(ble_buffer)); 
  delay(25);

  value = String(temperature);
  value = "Temp: " + value;
  value.toCharArray(ble_buffer, 64);
  bleuart.write(ble_buffer, strlen(ble_buffer));
  delay(25);
  
  value = String(humidity);
  value = "Hum:" + value;
  value.toCharArray(ble_buffer, 64);
  bleuart.write(ble_buffer, strlen(ble_buffer));
  delay(25);

  value = String(az);
  value = "Accel:" + value;
  value.toCharArray(ble_buffer, 64);
  bleuart.write(ble_buffer, strlen(ble_buffer));
  delay(25);
  
}
