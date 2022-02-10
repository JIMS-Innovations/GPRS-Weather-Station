/* This is code was written by Jesutofunmi Kupoluyi
for a weather station with the following features/components:

# Components:
* Arduino Mega
* RY-GH Pyranometer
* RS-485 Chip
* HTU31D ^2
* MAX6675
* K-type Thermocouple
* MH-Z19C CO2 sensor
* 0.96" OLED Screen
* SD Card Module
* SIM808 GSM + GPS module
# Features:
The weather station is expected to measure CO2 values using the MH-Z19C,
measure soil temperature using a K-type thermocouple and MAX6675 as its transciever,
measure humidity and temperature in at two given points (with two pieces) and
datalog this data onto a SD card,
upload the data also to a given website, and
display debugging info onto an OLED screen.
*/

// Including Libraries--------------------
#include <Arduino.h>
#include "SD.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_HTU31D.h"
#include "max6675.h"
#include "Wire.h"
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "MHZ19.h"
//#include <Ubidots.h>

// Variables-------------------------------
int temp1;
int temp2;
int hum1;
int hum2;
int soilTemp;
int co2;
int par;
int PAR_out;

// Refresh rates
uint16_t sensorRefreshRate = 500;
uint16_t datalogRefreshRate = 3000;
uint16_t uploadRefreshRate = 10000;

// Time Stamp variables
uint32_t sensorTimeStamp;
uint32_t datalogTimeStamp;
uint32_t uploadTimeStamp;

// PAR gain (Adjust as appropriate)
double par_gain = 392.5;

// PAR sensitivity (Adjust as appropriate)
double par_sensitivity = 12.56;

// PAR resolution (Adjust as apropriate)
int PAR_resoution = 100;

// PAR pin
const int PAR_pin = A0;

// Power LED-------------------------------
const int P_led = 23;

// Error LED-------------------------------
const int E_led = 25;

// Working LED-----------------------------
const int W_led = 27;

// Your GPRS credentials
#define UBI_EDUCATIONAL
String APN = "web.gprs.mtnnigeria.net";
String USER = "";
String PASS = "";
String TOKEN = "BBFF-5rFkhdKvXw2xBQp1N8jb5UkqPhidxj";
String DEVICE_LABEL = "weather-station";
String VARIABLE_LABEL_1 = "temperature";
String VARIABLE_LABEL_2 = "temperature2";
String VARIABLE_LABEL_3 = "humidity1";
String VARIABLE_LABEL_4 = "humidity2";
String VARIABLE_LABEL_5 = "new-variable-1";
String VARIABLE_LABEL_6 = "soil-temperature";
String VARIABLE_LABEL_7 = "radiation";

// Ubidots client(TOKEN, APN, USER, PASS);

// Keypad----------------------------------
const int UP = 2;
const int OK = 3;
const int DOWN = 10;

// SIM Power pin---------------------------
const int sim_pwr_pin = 9;

// SD Card setup---------------------------
const int chipSelect = 53;

// MAX6675 and Thermocouple setup----------
const int thermoDO = 4;
const int thermoCS = 5;
const int thermoCLK = 6;
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

// MH-Z19C setup---------------------------
#define BAUDRATE 9600 // Device to MH-Z19 Serial baudrate (should not be changed)
MHZ19 CO2;

// HTU31D setup-----1---------------------
Adafruit_HTU31D htu1 = Adafruit_HTU31D();

// HTU31D setup-----2---------------------
Adafruit_HTU31D htu2 = Adafruit_HTU31D();

// Pyranometer Setup--(for RS485 type)----
//#define DE 7
//#define RE 8

// OLED Setup-----------------------------
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

///////////////////////////////Custom functions///////////////////////////////////

// CO2--------------------------------------------------------
int CO_2()
{
  // MH-Z19C --------------------
  unsigned long Co2RefreshRate = 0;
  int co2;
  if (millis() - Co2RefreshRate >= 2000)
  {

    /* note: getCO2() default is command "CO2 Unlimited". This returns the correct CO2 reading even
    if below background CO2 levels or above range (useful to validate sensor). You can use the
    usual documented command with getCO2(false) */

    co2 = CO2.getCO2(); // Request CO2 (as ppm)

    Serial.print("CO2 (ppm): ");
    Serial.println(co2);

    Co2RefreshRate = millis();
  }
  return co2;
}

// Datalog------------------------------------------

void datalog(int temp1, int temp2, int hum1, int hum2, int co2, int thermocouple, int par)
{
  // SD card --------------------

  // Parsing data into a string variable
  String weatherData = "";
  weatherData += "Temperature1: ";
  weatherData += String(temp1);
  weatherData += "*C ";
  weatherData += " Temperature2: ";
  weatherData += String(temp2);
  weatherData += "*C ";
  weatherData += " Humidity1: ";
  weatherData += String(hum1);
  weatherData += "% ";
  weatherData += " Humidity2: ";
  weatherData += String(hum2);
  weatherData += "% ";
  weatherData += " CO2: ";
  weatherData += String(co2);
  weatherData += "ppm ";
  weatherData += " Thermocouple: ";
  weatherData += String(thermocouple);
  weatherData += "*C ";
  weatherData += " PAR: ";
  weatherData += String(par);
  weatherData += "u.mol.m^-2.s^-1 ";

  // Create a text file for the weather datalog
  File dataFile = SD.open("WDatalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile)
  {
    dataFile.println(weatherData);
    dataFile.close();
    // print to the serial port too:
    Serial.println(weatherData);
  }
  // if the file isn't open, pop up an error:
  else
  {
    Serial.println("error opening WDatalog.txt");
  }
}

// Atmospheric Temperature and Humidity------------------------

void tempHum(int &temp1, int &temp2, int &hum1, int &hum2)
{
  sensors_event_t humidity, temp;
  // HTU31 ------------------------ 1
  htu1.getEvent(&humidity, &temp);

  temp1 = temp.temperature;
  hum1 = humidity.relative_humidity;

  // HTU31 ------------------------ 2
  htu2.getEvent(&humidity, &temp);

  temp2 = temp.temperature;
  hum2 = humidity.relative_humidity;

  // Heater 1
  uint32_t timestamp1 = millis();
  bool heaterEnabled1 = false;
  if ((millis() - timestamp1) > 5000)
  {
    // toggle the heater
    heaterEnabled1 = !heaterEnabled1;
    if (heaterEnabled1)
    {
      Serial.println("Turning on heater1");
    }
    else
    {
      Serial.println("Turning off heater1");
    }
    if (!htu1.enableHeater(heaterEnabled1))
    {
      Serial.println("Command 1 failed");
    }

    timestamp1 = millis();
  }

  // Heater 2
  uint32_t timestamp2 = millis();
  bool heaterEnabled2 = false;
  if ((millis() - timestamp2) > 5000)
  {
    // toggle the heater
    heaterEnabled2 = !heaterEnabled2;
    if (heaterEnabled2)
    {
      Serial.println("Turning on heater 2");
    }
    else
    {
      Serial.println("Turning off heater 2");
    }
    if (!htu2.enableHeater(heaterEnabled1))
    {
      Serial.println("Command 2 failed");
    }

    timestamp2 = millis();
  }
}

// Pyranometer----------------------------------------------

int radiation()
{
  for (int i = 0; i < PAR_resoution; i++)
  {
    int radValue = analogRead(PAR_pin);
    int PAR_amp_volt = map(radValue, 0, 1023, 0, 5);
    int PAR_volt = PAR_amp_volt / par_gain;
    PAR_out = PAR_volt * par_sensitivity;
    PAR_out += PAR_out;
    delay(20);
  }

  return PAR_out / PAR_resoution;
}

// Gprs Upload-------------------------------------------------------
// For ThingSpeak API---------------------

void gprs(int temp1, int temp2, int hum1, int hum2, int co2, int soilTemp, int par)
{
  if (Serial2.available())
    Serial.write(Serial2.read());

  Serial2.println("AT");
  delay(1000);

  Serial2.println("AT+CPIN?");
  delay(1000);

  Serial2.println("AT+CREG?");
  delay(1000);

  Serial2.println("AT+CGATT?");
  delay(1000);

  Serial2.println("AT+CIPSHUT");
  delay(1000);

  Serial2.println("AT+CSTT=\"web.gprs.mtnnigeria.net\""); // start task and setting the APN,
  delay(1000);

  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  delay(5000);

  Serial2.println("AT+CIICR"); // bring up wireless connection
  delay(3000);

  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  delay(5000);

  Serial2.println("AT+CIFSR"); // get local IP adress
  delay(2000);

  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  delay(5000);

  Serial2.println("AT+CIPSPRT=0");
  delay(3000);

  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  delay(5000);

  Serial2.println("AT+CIPSTART=\"TCP\",\"things.ubidots.com\",80"); // start up the connection
  delay(6000);

  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  delay(5000);

  Serial2.println("AT+CIPSEND"); // begin send data to remote server
  delay(4000);

  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  delay(5000);

  String value = "{\"temperature\": " + String(temp1) + ", \"temperature2\": " + String(temp2) + ", \"humidity1\": " + String(hum1);
  String value2 = ", \"humidity2\": " + String(hum2) + ", \"soil-temperature\": " + String(soilTemp) + ", \"radiation\": " + String(par);
  String value3 = ", \"new-variable-1\": " + String(co2) + "}";
  int i = value.length();
  int j = value2.length();
  int k = value3.length();
  // String str = "GET https://api.thingspeak.com/update?api_key=QPK8OIBKF34IDKFT&field1=" + String(temp1) + "&field2=" + String(temp2) + "&field3=" + String(hum1) + "&field4=" + String(hum2) + "&field5=" + String(co2) + "&field6=" + String(soilTemp) + "&field7=" + String(par);
  String str1 = "POST /api/v1.6/devices/weather-station/ HTTP/1.1\r\n";
  String str2 = "Content-Type: application/json\r\nContent-Length: ";
  String str3 = String(i) + "\r\nX-Auth-Token: ";
  String str4 = TOKEN + "\r\n";
  String str5 = "Host: things.ubidots.com\r\n\r\n";
  // str+= value;
  // "User-Agent:" + USER_AGENT + "/" + VERSION + "\r\n"
  //"X-Auth-Token: " + *TOKEN + "\r\n"
  // "Connection: close\r\n"
  //"Content-Type: application/json\r\n"
  //"Content-Length: " + String(i) + "\r\n"
  // "\r\n"
  //+ value +
  //"\r\n";
  // Serial.println(str1 + str2 + str3 + str4 + value);
  // delay(1000);
  Serial2.println("POST /api/v1.6/devices/weather-station/ HTTP/1.1");
  delay(1000);
  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  Serial2.println("Host: things.ubidots.com");
  delay(1000);
  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  Serial2.print("X-Auth-Token: ");
  delay(1000);
  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  Serial2.println(TOKEN);
  delay(1000);
  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  Serial2.println("Content-Type: application/json");
  delay(1000);
  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  Serial2.println("Content-Length: " + String(i + j + k));
  delay(1000);
  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  Serial2.println("");
  Serial2.print(value);
  delay(2000);
  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  Serial2.print(value2);
  delay(2000);
  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  Serial2.println(value3);
  delay(2000);
  Serial2.println("");
  delay(1000);
  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  delay(1000);

  Serial2.println((char)26); // sending
  delay(5000);               // waitting for reply, important! the time is base on the condition of internet
  Serial2.println();
  
 if (Serial2.find("200"))
  {
    Serial.println("Upload Succesful!");

    digitalWrite(W_led, HIGH);
    delay(500);
    digitalWrite(W_led, LOW);
    delay(500);
  }
  else
  {
    Serial.println("Upload Error!");

    digitalWrite(E_led, HIGH);
    delay(250);
    digitalWrite(E_led, LOW);
    delay(250);
    digitalWrite(E_led, HIGH);
    delay(250);
    digitalWrite(E_led, LOW);
    delay(250);
  }
  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  delay(5000);

 

  Serial2.println("AT+CIPSHUT"); // close the connection
  delay(100);

  while (Serial2.available() != 0)
    Serial.write(Serial2.read());
  delay(5000);
}

////////////////////////////////////////Setup////////////////////////////

void setup()
{

  Serial.begin(BAUDRATE); // USB

  Serial2.begin(BAUDRATE); // GSM

  // Serial3.begin(9600); // PAR (for RS485 type)

  Serial1.begin(BAUDRATE); // CO2

  // client.setDebug(true); // Set true to make available debug messages
  CO2.begin(Serial1);
  CO2.autoCalibration();

  htu1.begin(0x40); // Sensor 1
  htu2.begin(0x41); // Sensor 2

  // Pin mode declaration
  pinMode(P_led, OUTPUT);
  pinMode(chipSelect, OUTPUT);
  pinMode(W_led, OUTPUT);
  pinMode(E_led, OUTPUT);
  pinMode(PAR_pin, INPUT);
  pinMode(UP, INPUT_PULLUP);
  pinMode(OK, INPUT_PULLUP);
  pinMode(DOWN, INPUT_PULLUP);

  digitalWrite(P_led, HIGH);

  SD.begin(chipSelect);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
  }
  sensorTimeStamp = millis();
  datalogTimeStamp = millis();
  uploadTimeStamp = millis();

  File dataFile = SD.open("WDatalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile)
  {
    dataFile.println("---------------------------------------New Reading-----------------------------------------------");
    dataFile.close();
    // print to the serial port too:
    Serial.println("New Reading");
  }
  // if the file isn't open, pop up an error:
  else
  {
    Serial.println("error opening WDatalog.txt");
  }

  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
}

//////////////////////////////////////////Loop/////////////////////////////

void loop()
{

  //////////////////////Sensors loop///////////////////////////////
  // if((millis() - sensorTimeStamp) > sensorRefreshRate){
  // Thermocouple --------------------
  Serial.print("Soil Temperature = ");
  soilTemp = thermocouple.readCelsius();
  Serial.println(soilTemp);

  // CO2------------------------------
  Serial.print("Carbondioxide = ");
  co2 = CO_2();
  Serial.println(co2);

  // Atmospheric Temperature and Humidity
  tempHum(temp1, temp2, hum1, hum2);
  Serial.println("Temperature 1 = " + String(temp1) + "*C " + " Humidity 1 = " + String(hum1) + "%");
  Serial.println("Temperature 2 = " + String(temp2) + "*C " + " Humidity 2 = " + String(hum2) + "%");

  // Solar Radiation-------------------
  par = radiation();
  Serial.println("Solar Radiation = " + String(par));

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Temperature 1 = " + String(temp1) + "*C " + " Humidity 1 = " + String(hum1) + " % " + " Temperature 2 = " + String(temp2) + "*C " + " Humidity 2 = " + String(hum2) + "% " + " Soil Temperature = " + String(soilTemp) + " Carbondioxide = " + String(co2) + " Solar Radiation = " + String(par));
  display.display();
  delay(sensorRefreshRate);
  // sensorTimeStamp = millis();
  //}

  ////////////////////Datalogging loop/////////////////////
  // if((millis() - datalogTimeStamp) > datalogRefreshRate){
  // Datalogging-------------------------
  datalog(temp1, temp2, hum1, hum2, co2, soilTemp, par);

  delay(datalogRefreshRate);
  // datalogTimeStamp = millis();
  //}

  //////////////////////Upload loop/////////////////////
  // if((millis() - uploadTimeStamp) > uploadRefreshRate){
  // Data upload-------------------------
  gprs(temp1, temp2, hum1, hum2, co2, soilTemp, par);
  // client.add(VARIABLE_LABEL_1, temp1);
  // client.add(VARIABLE_LABEL_2, temp2);
  // client.add(VARIABLE_LABEL_3, hum1);
  // client.add(VARIABLE_LABEL_4, hum2);
  // client.add(VARIABLE_LABEL_5, co2);
  // client.add(VARIABLE_LABEL_6, soilTemp);
  // client.add(VARIABLE_LABEL_7, par);

  // if (Serial.find("OK"))
  // {
  //   Serial.println("Data sent to Ubidots sucessfully!");
  // }
  //
  delay(5000);
  // uploadTimeStamp = millis();
  //}
}