#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <OneWire.h>
#include <math.h>
#include <ESP8266Ping.h>

// Define WiFi Router credentials
#ifndef STASSID
#define STASSID "SSID"
#define STAPSK  "password"
#endif

#define NODEID "2BED1_THERM"

// Define temperature limits
#define TEMPMIN 15
#define TEMPMAX 30

#define SCL D1
#define SDA D2

#define TEMP_SENSOR_PIN D3
#define MOTION_SENSOR_PIN D5

//Define Rotary encoder pins
#define encoder0PinA  D6
#define encoder0PinB  D7

// Define timers
#define TEMPERATURE_TIMER 1ul
#define BACKLIGHT_TIMER 30ul
#define SECONDS_CONST 1000ul //Seconds constant

// Declare WiFiClient object
WiFiClient client;
// Set WiFi Router SSID and password
const char* ssid = STASSID;
const char* password = STAPSK;

const IPAddress server_ip(*, *, *, *);
//const char* server_ip = "raspberrypi";

// Declare PubSubClient object
PubSubClient pubSubClient(client);
bool brokerOnline = true;

const char* mqtt_server = "*.*.*.*";
char publishMessage[20];
char temp_value_holder[5];

// Rotary encoder initial value
volatile int encoder0Pos = 25;
volatile boolean PastB = 0;
int set_temp = 20;

// Temperature calculation variables
OneWire  ds(TEMP_SENSOR_PIN);
bool temperatureFetched = true;
bool temperatureReset = false;
byte i;
byte present = 0;
byte type_s;
byte data[12];
byte addr[8];
float celsius, temperature;

// Time counters
unsigned long encoder_timer = 0;
unsigned long last_encoder_timer = 0;
unsigned long resetTemperature_time = 0;
unsigned long requestTemperature_time = 0;
unsigned long temperature_time = 0;
unsigned long motion_timer = 0;
unsigned long publish_timer = 0;
unsigned long ping_timer = 0;

// Motion sensor variable
int motion;

// Define LCD Display
//LiquidCrystal_I2C lcd(0x3F, 16, 2);
LiquidCrystal_I2C lcd(0x3F, 20, 4);

void setup() {
  Serial.begin(115200);

  WiFi.hostname(NODEID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
    
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(NODEID);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  
  ArduinoOTA.begin();
  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Ready");

  //Set up MQTT server connection
  pubSubClient.setServer(mqtt_server, 1883);
  pubSubClient.setCallback(callback);
  
  // Initialize LCD
  Wire.begin(SDA, SCL);
  lcd.init();
  lcd.backlight();

  // Initialize and attach an interrupt
  pinMode(encoder0PinA, INPUT_PULLUP);
  //digitalWrite(encoder0PinA, HIGH);
  pinMode(encoder0PinB, INPUT_PULLUP);
  //digitalWrite(encoder0PinB, HIGH);
  attachInterrupt(digitalPinToInterrupt(encoder0PinA), doEncoderB, RISING);

  // Calculate temperature
  resetTemp();
  delay(250);
  requestTemp();
  delay(800);
  temperature = fetchTemp()-0.5;

  //Display temperature
  lcd.setCursor(0,0);
  lcd.print("Actual: ");
  lcd.print(temperature, 1);
  lcd.print(" ");
  lcd.print((char)223);
  lcd.print("C ");
  lcd.setCursor(0,1);
  lcd.print("Set: ");
  lcd.print(encoder0Pos);
  lcd.print(" ");
  lcd.print((char)223);
  lcd.print("C ");
  lcd.setCursor(0,2);
  lcd.print("I MISS YOU JONIM");
  lcd.setCursor(0,3);
  lcd.print(WiFi.hostname());
}

void loop() {
  
  ArduinoOTA.handle();
  pubSubClient.loop();

  // If MQTT server online - publish set and actual temperatures,
  // otherwise, update screen to indicate server offline
  if (brokerOnline && ((millis() - publish_timer) > 5000)) {
    if (!pubSubClient.connected()) {
      if (!pubSubClient.connect(NODEID)) {
        brokerOnline = false;
//        lcd.setCursor(12,1);
        lcd.setCursor(16,3);
        lcd.print("OFFL");
      }
    } else {
      sprintf(temp_value_holder, "%.1f", (float)encoder0Pos);
      strcat(publishMessage, temp_value_holder);
      strcat(publishMessage, ",");
      sprintf(temp_value_holder, "%.1f", temperature);
      strcat(publishMessage, temp_value_holder);
      pubSubClient.publish(NODEID, publishMessage);
      publishMessage[0] = '\0';
    }

//    pubSubClient.subscribe("SET_2BED1");
    pubSubClient.loop();
    publish_timer = millis();
  }

  // ping the broker to verify we're still online
  pingBroker();

  // check and update temperature
  if(millis()-temperature_time > (TEMPERATURE_TIMER*SECONDS_CONST)) {
     //check last temperature fetched and sensor not yet reset
     if(temperatureFetched && !temperatureReset){
        resetTemp();
     //check last temperature fetched, sensor reset and 250 ms passed since reset
     }else if(temperatureFetched && temperatureReset && (millis() - resetTemperature_time > 250)){
        requestTemp();
     //check new temperature not fetched, sensor reset and 1000 ms passed since temperature measure request - if all true, fetch new temperature   
     }else if(!temperatureFetched && temperatureReset && (millis() - requestTemperature_time > 1000)){
        temperature = fetchTemp()-0.5;
        updateScreen("act");
        temperature_time = millis();
        temperatureFetched = true;
        temperatureReset = false;
     }
  }

   // Update the "set" temp on LCD
   updateScreen("set");
   
   // Check for motion
   checkMotion();
}

// callback function to execute when there's a new message for the topic we subscribed
void callback(char* topic, byte* payload, unsigned int length) {
  
  for (int i = 0; i < 2; i++) {
    temp_value_holder[i] = (char)payload[i];
  }
  temp_value_holder[2] = '\0';
  set_temp = atof(temp_value_holder);

  if (set_temp >= 15 && set_temp <= 32){
    encoder0Pos = set_temp;
  }
  brokerOnline = true;
}

// ping the MQTT server to verify it's online
void pingBroker(){
  if (!brokerOnline && ((millis()-ping_timer) > 10000)){  
    brokerOnline = Ping.ping(server_ip);
    if (brokerOnline){
//      lcd.setCursor(12,1);
      lcd.setCursor(16,3);
      lcd.print("    ");
    }
    ping_timer = millis();
  }
}

// rotary encoder interrupt function
ICACHE_RAM_ATTR void doEncoderB()
{
   encoder_timer = millis();
   
   if (encoder_timer - last_encoder_timer > 6)
   {     
      PastB=(boolean)digitalRead(encoder0PinB);
      if(!PastB && (encoder0Pos < TEMPMAX)){
         encoder0Pos++;  
      }else if (PastB && (encoder0Pos > TEMPMIN)){
         encoder0Pos--;
      }
      motion_timer = millis();
   }
   last_encoder_timer = encoder_timer;
}

// functions to periodically check for motion sensor
void checkMotion(){
   motion = digitalRead(MOTION_SENSOR_PIN);
   
   if(motion){
      motion_timer = millis();
   }
   
   if((millis()-motion_timer) > (BACKLIGHT_TIMER*SECONDS_CONST)){
      lcd.noBacklight();
   }else{
      lcd.backlight();
   }
}

// function to update the screen with new set or actual temperature
void updateScreen(char* dataPtr){
   if(strcmp(dataPtr, "act") == 0){
    lcd.setCursor(8,0);
    lcd.print(temperature, 1);
   }else if (strcmp(dataPtr, "set") == 0){
    lcd.setCursor(5,1);
    lcd.print(encoder0Pos);
   }
}

// reset the temp sensor
void resetTemp()
{
   resetTemperature_time = millis();
   temperatureReset = true;
   if ( !ds.search(addr)){
      ds.reset_search();
      return;
   }
}

// initiate a new sensor reading calculation
void requestTemp(){
   
   requestTemperature_time = millis();
   temperatureFetched=false;
   
   if (OneWire::crc8(addr, 7) != addr[7])
   {
      return;
   }

   //the first ROM byte indicates which chip
   switch (addr[0]) {
      case 0x10:
         type_s = 1;
         break;
      case 0x28:
         type_s = 0;
         break;
      case 0x22:
         type_s = 0;
         break;
      default:
         return;
   }

   ds.reset();
   ds.select(addr);
   ds.write(0x44, 1);        // start conversion, with parasite power on at the end
}

// fetch the most recent calculated temperature
float fetchTemp()
{
   present = ds.reset();
   ds.select(addr);    
   ds.write(0xBE);         // Read Scratchpad

   for ( i = 0; i < 9; i++)
   {           // we need 9 bytes
      data[i] = ds.read();
   }
  
   // Convert the data to actual temperature
   // because the result is a 16 bit signed integer, it should
   // be stored to an "int16_t" type, which is always 16 bits
   // even when compiled on a 32 bit processor.
   int16_t raw = (data[1] << 8) | data[0];
   if (type_s)
   {
      raw = raw << 3; // 9 bit resolution default
      if (data[7] == 0x10)
      {
         // "count remain" gives full 12 bit resolution
         raw = (raw & 0xFFF0) + 12 - data[6];
      }
   }else{
      byte cfg = (data[4] & 0x60);
      // at lower res, the low bits are undefined, so let's zero them
      if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
      else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
      else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
      //// default is 12 bit resolution, 750 ms conversion time
   }
   celsius = (float)raw / 16.0;
   return celsius;
}
