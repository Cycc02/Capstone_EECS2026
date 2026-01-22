#include <stdio.h>
#include <vector>
#include <Wire.h>
#include <SPI.h>
#include <string.h>
#include <LoRa.h>

#define MAX_ACC_SAMPLES 100

TwoWire tempwire = TwoWire(1);

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

int16_t acc_x[MAX_ACC_SAMPLES];
int16_t acc_y[MAX_ACC_SAMPLES];
int16_t acc_z[MAX_ACC_SAMPLES];
int acc_idx = 0;

void setup()
{
  delay(10);
  Serial.begin(115200);
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
  
  /*
  Serial.println("Initializing LoRa");
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DI0);
  LoRa.setSPI(SPI);
  
  if (!LoRa.begin(433E6)) { 
    Serial.println("LoRa init failed.");
    while (true);
  }
  LoRa.enableCrc();
  Serial.println("LoRa Initialized!");
  */
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
void loop()
{
  static unsigned long acc_read = 0; //Initialize once only
  static String lux_cond = "N/A";
  static String temp_cond = "N/A";

  if(acc_idx < MAX_ACC_SAMPLES)
  {
    if(millis() - acc_read >= 10)
    {
      uint8_t raw[6];
      SPI_ReadMulti(0x32,raw,6);
      int16_t x = ((int16_t)raw[1] << 8 | raw[0]);
      int16_t y = ((int16_t)raw[3] << 8 | raw[2]);
      int16_t z = ((int16_t)raw[5] << 8 | raw[4]);
      acc_x[acc_idx] = x;
      acc_y[acc_idx] = y;
      acc_z[acc_idx] = z;

      acc_idx++;
      acc_read = millis();
    }
  }
  else
  {
    float lux = readlux_continuous(LIGHT_ADDR);
    float temp = readtemp_continuous(TEMP_CONFIG,TEMP_ADDR);
   
    if(lux == 0.0f){
      lux_cond = "ERROR";   
    }
    else {
      lux_cond = String(lux,2);
    }

    if(temp == 0.0f){
      temp_cond = "ERROR";   
    }
    else {
      temp_cond = String(temp,2);
    }

    Serial.printf("%.2f, %.2f", temp, lux);
    for (int i = 0; i < MAX_ACC_SAMPLES; i++) {
      Serial.printf(",%d,%d,%d", acc_x[i], acc_y[i], acc_z[i]);
    }
    Serial.println();
    acc_idx = 0;
  }
}

