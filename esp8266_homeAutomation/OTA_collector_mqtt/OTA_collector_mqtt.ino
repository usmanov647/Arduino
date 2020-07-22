#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <math.h>
#include <ESP8266Ping.h>

#ifndef STASSID
#define STASSID "SSID"
#define STAPSK  "password"
#endif

#define NODEID "1COLLECTOR"

#define SCL D1
#define SDA D2

#define CH1 D3
#define CH2 D4
#define CH3 D5
#define CH4 D6
#define CH5 D7
#define CH6 D8
#define CH7 10
#define CH8 9

// Define timers
#define CHECK_MQTT_TIMER 3ul
#define SECONDS_CONST 1000ul //Seconds constant

// Declare WiFiClient object
WiFiClient client;
// Set WiFi Router SSID and password
const char* ssid = STASSID;
const char* password = STAPSK;

const IPAddress server_ip(*, *, *, *);
//const char* remote_ip = "raspberrypi";

// Declare PubSubClient object
PubSubClient pubSubClient(client);
const char* mqtt_server = "*.*.*.*";
char publishMessage[20];
char temp_value_holder[5];
bool brokerOnline = true;

uint8_t SpecialChar [8] = { 0x00, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00 };

int cursor_position[][2] = {{0,0}, {11,0}, {0,1}, {11,1}, {0,2}, {11,2}, {0,3}, {11,3}};
char *zones[] = {"1BATH", "1BED1", "1LIVI", "ENTRA", "1HALL", "KITCH"}; //first floor
//char *zones[] = {"2BATH", "2BED1", "2LIVI", "2BED2", "2BED3"};  //second floor
int contours[][3] = {{CH1, CH1, CH1}, {CH2, CH2, CH2}, {CH3, CH3, CH3}, {CH4, CH4, CH4}, {CH5, CH6, CH7}, {CH8, CH8, CH8}}; //first floor
//int contours[][3] = {{CH1, CH1, CH1}, {CH2, CH2, CH2}, {CH3, CH4, CH4}, {CH5, CH6, CH6}, {CH7, CH7, CH7}}; //second floor
int zone_status[] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH}; //first floor
//int zone_status[] = {HIGH, HIGH, HIGH, HIGH, HIGH}; //second floor
unsigned long nodes_timeout[] = {0, 0, 0, 0, 0, 0};
//unsigned long nodes_timeout[] = {0, 0, 0, 0, 0};

unsigned long check_mqtt_timer = 0;
unsigned long ping_timer = 0;
unsigned long flow_animation_timer = 0;

int flow_indicator = 0;
float set_temp = 0.0;
float actual_temp = 0.0;

// Define LCD Display
//LiquidCrystal_I2C lcd(0x27, 20, 4);
LiquidCrystal_I2C lcd(0x3F, 20, 4);

void setup() {
  Serial.begin(115200);

  pinMode(CH1, OUTPUT);
  pinMode(CH2, OUTPUT);
  pinMode(CH3, OUTPUT);
  pinMode(CH4, OUTPUT);
  pinMode(CH5, OUTPUT);
  pinMode(CH6, OUTPUT);
  pinMode(CH7, OUTPUT);
  pinMode(CH8, OUTPUT);

  for(int zone = 0; zone < (sizeof(zones)/sizeof(zones[zone])); zone++){
    updateRelayStatus(zone);
  }

  Serial.println("Booting");

  WiFi.hostname(NODEID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(500);
    ESP.restart();
  }

  pubSubClient.setServer(mqtt_server, 1883);
  pubSubClient.setCallback(callback);

  ArduinoOTA.setHostname(NODEID);

  ArduinoOTA.onError([](ota_error_t error) {
    (void)error;
    ESP.restart();
  });

  /* setup the OTA server */
  ArduinoOTA.begin();

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Ready");

  // Initialize LCD
  Wire.begin(SDA, SCL);
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, SpecialChar);

  for(int i = 0; i < (sizeof(zones)/sizeof(zones[0])); i++){
    lcd.setCursor(cursor_position[i][0],cursor_position[i][1]);
    lcd.print(zones[i]);
  }
}
 
void loop() {
  
  ArduinoOTA.handle();
  pubSubClient.loop();

  if (brokerOnline && ((millis() - check_mqtt_timer) > CHECK_MQTT_TIMER*SECONDS_CONST)) {
    if (!pubSubClient.connected()) {
      if (!pubSubClient.connect(NODEID)) {
        brokerOnline = false;
      }
    } else {
//      pubSubClient.subscribe("2BATH_THERM");
//      pubSubClient.subscribe("2BED1_THERM");
//      pubSubClient.subscribe("2BED2_THERM");
//      pubSubClient.subscribe("2BED3_THERM");
//      pubSubClient.subscribe("2LIVI_THERM");
      pubSubClient.subscribe("1BATH_THERM");
      pubSubClient.subscribe("1BED1_THERM");
      pubSubClient.subscribe("1LIVI_THERM");
      pubSubClient.subscribe("1HALL_THERM");
      pubSubClient.subscribe("ENTRA_THERM");
      pubSubClient.subscribe("KITCH_THERM");
      pubSubClient.loop();
    }

    check_mqtt_timer = millis();
  }

  pingBroker();
  flowAnimation();

  for(int zone = 0; zone < (sizeof(zones)/sizeof(zones[zone])); zone++){
    if((millis() - nodes_timeout[zone]) > 60000){
      updateScreen(zone, 88.0);
      zone_status[zone] = HIGH;
      updateRelayStatus(zone);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
    
  for (int i = 0; i < 4; i++) {
    temp_value_holder[i] = (char)payload[i];
  }
  temp_value_holder[4] = '\0';
  set_temp = atof(temp_value_holder);

  for (int j = 5; j < 9; j++) {
    temp_value_holder[j-5] = (char)payload[j];
  }
  temp_value_holder[4] = '\0';
  actual_temp = atof(temp_value_holder);

  for(int zone = 0; zone < (sizeof(zones)/sizeof(zones[zone])); zone++){
    if(strstr(topic, zones[zone])){
      checkTemperature(zone, set_temp, actual_temp);
      nodes_timeout[zone] = millis();
    }
  }

  if(strcmp(topic, "broker_online") == 0){
    brokerOnline = true;
  }
}

void pingBroker(){
  if (!brokerOnline && ((millis()-ping_timer) > 10000)){  
    brokerOnline = Ping.ping(server_ip);
    if (brokerOnline){
      lcd.setCursor(16,3);
      lcd.print("    ");
    }
    ping_timer = millis();
  }
}

void checkTemperature(int zone, double temp_set, double temp_actual){
  if((temp_set >= 15.0) && (temp_set <= 30.0) && (temp_actual > 0.0) && (temp_actual < 50.0)){
    if((temp_actual-temp_set) > 0.5){
      zone_status[zone] = LOW;
    }else if((temp_actual-temp_set) < -0.5){
      zone_status[zone] = HIGH;
    }
    updateScreen(zone, set_temp);
  }else{
    zone_status[zone] = HIGH;
    updateScreen(zone, 99.0); //error - invalid temperature
  }
  updateRelayStatus(zone);
}

void updateScreen(int zone, float temp){
  lcd.setCursor((cursor_position[zone][0]+6),(cursor_position[zone][1]));
  if(temp == 99.0){
    lcd.print("ERR");
  }else if(temp == 88.0){
    lcd.print("OFF");
  }else{
    lcd.print(" ");
    lcd.print(temp, 0); 
  }
}

void updateRelayStatus(int zone){
  for(int contour = 0; contour < (sizeof(contours[zone])/sizeof(contours[zone][0])); contour++){
    digitalWrite(contours[zone][contour], zone_status[zone]);
  }
}

void flowAnimation(){

  if(millis() - flow_animation_timer > 500){
    for(int zone = 0; zone < (sizeof(zones)/sizeof(zones[0])); zone++){
      lcd.setCursor((cursor_position[zone][0]+5),(cursor_position[zone][1]));
      if(zone_status[zone] == HIGH){
        switch(flow_indicator){
          case 0:
            lcd.print("|");
            break;
          case 1:
            lcd.print("/");
            break;
          case 2:
            lcd.print("-");
            break;
          case 3:
            lcd.print(char(0));
            break;
        }
      }else{
        lcd.print("*");
      }
    }
    if(flow_indicator == 3){
      flow_indicator = 0;
    }else{
      flow_indicator++;
    }
    
    flow_animation_timer = millis();
  }
}
