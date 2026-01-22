#include <stdio.h>
#include <vector>
#include <Wire.h>
#include <SPI.h>
#include <string.h>
#include <LoRa.h>
#include <math.h>

#include "feature_extraction.h"
#include "failure_classifier_rf.h"

TwoWire tempwire = TwoWire(1);

const float SCALER_MEAN[] = { 40.716721, 0.339038, 40.274526, 41.199036, 0.006372, 0.023844, 0.129995, 28975.297168, 2440.385468, 26219.621564, 31508.388423, -18.075541, -54.128488, -0.234129, -1.066659, 13.042338, -37.071168, 37.035765, 74.106351, 24.580453, -0.015117, -27.765694, 14.119658, -64.211063, 13.092985, 77.304749, 37.616982, -0.024970, 199.966752, 13.901340, 161.104618, 241.065408, 79.961245, 201.162635, -0.005749, 208.004173, 13.063437, 252.539680 };
const float SCALER_SCALE[] = { 5.363918, 0.434325, 4.967493, 5.810911, 0.254417, 1.011996, 1.065227, 10596.523870, 5624.071903, 14511.606994, 9293.671836, 2009.858471, 10778.865377, 1.094870, 21.443735, 22.186227, 65.582847, 64.071396, 118.255874, 22.783730, 0.489555, 21.163763, 24.997731, 62.369791, 79.050143, 131.430954, 25.060920, 0.568610, 8.047630, 16.433024, 55.292737, 58.972867, 110.053688, 6.956563, 0.258289, 5.024725, 14.819034, 71.768001 };
const char* AUTH_TOKEN = "O66W2EftD9t3gSSiBA86wAMcrUAKnHJ_";
//Classifier Object
Eloquent::ML::Port::RandomForest clf;

//Define Memory (RTC = Survives Sleep)
#define NIGHT_LOOPS 144 //12hours with 5mins interval
#define DAY_SLEEP_HRS 12
#define WINDOW_SIZE 5
RTC_DATA_ATTR float temp_hist[WINDOW_SIZE];
RTC_DATA_ATTR float light_hist[WINDOW_SIZE];
RTC_DATA_ATTR bool calibrate_flag = true;
RTC_DATA_ATTR float light_failure_threshold = 0.0;
RTC_DATA_ATTR float light_instability_threshold = 0.0;
RTC_DATA_ATTR float overheat_threshold = 0.0;

#define MAX_ACC_SAMPLES_PER_AXIS 100

const int LIGHT_SCL = 22;
const int LIGHT_SDA = 21;
const int LIGHT_ADDR = 0x23;

const int TEMP_SCL = 32;
const int TEMP_SDA = 33;
const uint8_t TEMP_ADDR = 0x5A;
const uint8_t TEMP_CONFIG = 0x07; //Object Sensing

const int SPI_SDA = 12;
const int SPI_CS = 15;
const int SPI_SDO = 13;
const int SPI_CLK = 14;

const int LORA_CS = 4;
const int LORA_DI0 = 16;
const int LORA_RST = 27;

const int BATTERY_PIN = 34;
const int STARTUP_LIGHT_TRIGGER = 10000;

float acc_x[MAX_ACC_SAMPLES_PER_AXIS];
float acc_y[MAX_ACC_SAMPLES_PER_AXIS];
float acc_z[MAX_ACC_SAMPLES_PER_AXIS];
float acc_mag[MAX_ACC_SAMPLES_PER_AXIS];

int last_result = -1;

void setup()
{
  Serial.begin(115200);
  delay(10);
  Serial.println("Initializing SPI");

  SPI.begin(SPI_CLK, SPI_SDA, SPI_SDO, SPI_CS);
  pinMode(SPI_CS, OUTPUT);
  pinMode(LORA_CS, OUTPUT);
  digitalWrite(LORA_CS, HIGH);
  digitalWrite(SPI_CS, HIGH);

  delay(5);
  SPI_Write(0x2D, 0x08); //POWER_CTL: Measure Mode
  delay(5);
  SPI_Write(0x31, 0x08); //Data format  + Resolution
  delay(5);
  SPI_Write(0x2c, 0X0A); //Output Data Rate: 100Hz
  delay(5);
  
  Serial.println("SPI Initialization Success");
  Serial.println("Initializing I2C");

  Wire.setPins(LIGHT_SDA, LIGHT_SCL);
  Wire.begin(LIGHT_SDA, LIGHT_SCL, 50000);

  Wire.beginTransmission(LIGHT_ADDR);
  Wire.write(0x01);  // Power ON
  Wire.endTransmission();

  delay(10);

  Wire.beginTransmission(LIGHT_ADDR);
  Wire.write(0x07);  // Reset (optional)
  Wire.endTransmission();

  delay(10);

  Wire.beginTransmission(LIGHT_ADDR);
  Wire.write(0x10); //Light Sensor Continuous Mode
  Wire.endTransmission();

  delay(200);

  tempwire.setPins(TEMP_SDA, TEMP_SCL);
  tempwire.begin(TEMP_SDA, TEMP_SCL, 50000);

  delay(10);

  tempwire.beginTransmission(TEMP_ADDR);
  tempwire.write(0x00);
  tempwire.endTransmission();

  delay(10);
  Serial.println("I2C Initialization Success");
  
  Serial.println("Initializing LoRa");
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DI0);
  LoRa.setSPI(SPI);
  if (!LoRa.begin(433E6)) { 
    Serial.println("LoRa init failed. Check connections. Retrying....");
    while (true);
  }
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  Serial.println("LoRa Initialization Success");

  if(calibrate_flag == true){
    Serial.println("Waiting for Light Activation....");
    while(readlux_continuous(LIGHT_ADDR) < STARTUP_LIGHT_TRIGGER){};
    Serial.println("Calibrating Sensor......");
    auto_callibration();
    Serial.println("Done Calibration");
    calibrate_flag = false;
  }

  pinMode(BATTERY_PIN, INPUT);
  
}

float Battery_Read(){
  int rawADC = analogRead(BATTERY_PIN);
  //Serial.printf("Battery Analog Read: %d \n", rawADC);
  float pinVoltage = (rawADC / 4095.0) * 3.3;
  return (pinVoltage * 2);
}

void SPI_Write(uint8_t reg, uint8_t value)
{
  SPI.beginTransaction(SPISettings(5000000, MSBFIRST, SPI_MODE3));
  digitalWrite(SPI_CS, LOW);
  SPI.transfer(reg);
  SPI.transfer(value);
  digitalWrite(SPI_CS, HIGH);
  SPI.endTransaction();
}

void SPI_ReadMulti(uint8_t reg, uint8_t *buffer, uint8_t len)
{
  SPI.beginTransaction(SPISettings(5000000, MSBFIRST, SPI_MODE3));
  digitalWrite(SPI_CS, LOW);
  SPI.transfer(0xC0|reg);
  for(int i = 0; i < len; i++)
  {
    buffer[i] = SPI.transfer(0x00);
  }
  digitalWrite(SPI_CS, HIGH);
  SPI.endTransaction();
}

float readlux_continuous(uint8_t address)
{
  Wire.requestFrom(address, (uint8_t)2);
  
  if (Wire.available() == 2)
  {
    uint16_t raw = (Wire.read() << 8) | Wire.read();
    float lux = raw/1.2f;
    return lux;
  }
  return 0.0f;
}

float readtemp_continuous(uint8_t reg, uint8_t address)
{
  tempwire.beginTransmission(address);
  tempwire.write(reg);
  tempwire.endTransmission(false);

  delayMicroseconds(300);

  tempwire.requestFrom(address, (uint8_t)3);
  if (tempwire.available() == 3)
  {
    uint8_t lsb = tempwire.read();
    uint8_t msb = tempwire.read();
    uint16_t temp_raw = (msb << 8) | lsb;
    tempwire.read(); //PEC ignored
    float temp = (temp_raw * 0.02f) - 273.15f;
    return temp;
  }
  return 0.0f;

}

void auto_callibration(){
  int light_sum = 0;
  int samples = 0;
  float temp_sum = 0;

  unsigned long start = millis();
  while(millis() - start < 60000){
    light_sum += readlux_continuous(LIGHT_ADDR);
    temp_sum += readtemp_continuous(TEMP_CONFIG, TEMP_ADDR);
    samples++;
    delay(100);
  }

  float light_avg = light_sum / samples;
  float temp_avg = temp_sum / samples;

  light_failure_threshold = light_avg * 0.5;
  light_instability_threshold = light_avg * 0.8;
  overheat_threshold = temp_avg * 1.2;
  //Monitoring 
  Serial.printf("Avg Light: %.2f | Avg Temp: %.2f \n", light_avg, temp_avg);
}

void loop() {
  // --- Sampling Window ---
  for(int i = 0; i < MAX_ACC_SAMPLES_PER_AXIS; i++){
      uint8_t raw[6];
      SPI_ReadMulti(0x32, raw, 6);
      
      int16_t x = ((int16_t)raw[1] << 8 | raw[0]);
      int16_t y = ((int16_t)raw[3] << 8 | raw[2]);
      int16_t z = ((int16_t)raw[5] << 8 | raw[4]);
      
      acc_x[i] = (float)x;
      acc_y[i] = (float)y;
      acc_z[i] = (float)z;
      acc_mag[i] = sqrt(pow(acc_x[i],2) + pow(acc_y[i],2) + pow(acc_z[i],2));
      
      delay(10); 
  }

  // Read sensors
  float cur_light = readlux_continuous(LIGHT_ADDR);
  float cur_temp  = readtemp_continuous(TEMP_CONFIG, TEMP_ADDR);

  // Update rolling history
  for(int i = 0; i < WINDOW_SIZE - 1; i++) {
      temp_hist[i]  = temp_hist[i+1];
      light_hist[i] = light_hist[i+1];
  }
  temp_hist[WINDOW_SIZE-1]  = cur_temp;
  light_hist[WINDOW_SIZE-1] = cur_light;

  // Extract features
  float raw_features[38];
  extract_feature(raw_features, 
                  acc_x, acc_y, acc_z, acc_mag, MAX_ACC_SAMPLES_PER_AXIS,
                  temp_hist, light_hist, WINDOW_SIZE,
                  cur_temp, cur_light);

  // Scale
  float scaled_features[38];
  for(int i = 0; i < 38; i++){
    scaled_features[i] = (raw_features[i] - SCALER_MEAN[i]) / SCALER_SCALE[i]; 
  }

  // Predict
  int result = clf.predict(scaled_features);

  delay(10);
  yield();
    
  //Sensor Monitoring
  Serial.printf("Light: Current %.2f vs Threshold %.2f\n", cur_light, light_failure_threshold);
  Serial.printf("Temp: Current %.2f vs Threshold %.2f\n", cur_temp, overheat_threshold);

  // Safety filter for false shock
  if(cur_light < light_failure_threshold){
    result = 0;
  }
  else if (cur_light < light_instability_threshold){
    result = 3;
  }
  else if (cur_temp > overheat_threshold){
    result = 5;
  }
  else{
    if(result == 2) {
    float z_std   = raw_features[29];
    float z_range = raw_features[32];

    if(z_range < 12 && z_std < 2.5) {
        result = 4;   // force NORMAL
      }
    }
    else if (result == 0 || result == 3){
      if (cur_light > light_instability_threshold){result = 4;}
      else if (cur_light > light_failure_threshold && cur_light < light_instability_threshold) {result = 3;}
    }
    else if(result == 5){
      if(cur_temp < overheat_threshold) {result = 4;}
    }
  }

  //Monitor Battery
  float battery_voltage = Battery_Read();
  Serial.printf("Battery Voltage: %.2f \n", battery_voltage);
  

  if(true){
    digitalWrite(SPI_CS, HIGH);
    LoRa.beginPacket();
    LoRa.print(AUTH_TOKEN);
    LoRa.print(",");
    LoRa.print(result);
    LoRa.print(",");
    LoRa.print(battery_voltage);
    LoRa.endPacket();
    Serial.println("Sent to LoRa");
    delay(2000);
  }

  // Output ONLY CLASS
  Serial.print("Class: ");
  Serial.println(result);
}

