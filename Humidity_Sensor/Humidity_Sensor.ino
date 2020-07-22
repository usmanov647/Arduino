#include <avr/wdt.h>
#include <DHT.h>;

#define RED 10
#define GREEN 11
#define BLUE 3
#define BUTTON_PIN 2
#define DHTPIN 12
#define RELAY_PIN 13
#define DHTTYPE DHT22   // DHT 22  (AM2302)

DHT dht(DHTPIN, DHTTYPE); //// Initialize DHT sensor for normal 16mhz Arduino

unsigned long button_time = 0;
unsigned long last_button_time = 0;
unsigned long humid_measure_time = 0;
float hum;
int mode = 1;

void setup()
{
   wdt_enable(WDTO_8S);
   
   pinMode(BUTTON_PIN, INPUT);
   digitalWrite(BUTTON_PIN, HIGH);
   
   pinMode(RED, OUTPUT);
   pinMode(GREEN, OUTPUT);
   pinMode(BLUE, OUTPUT);
   pinMode(RELAY_PIN, OUTPUT);

   dht.begin();
   hum = dht.readHumidity();

   attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonPress, LOW);
}

void loop()
{
   if(mode == 0){
      analogWrite(RED, 0);
      analogWrite(GREEN, 0);
      analogWrite(BLUE, 150);
      detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
      delay(5);
      digitalWrite(RELAY_PIN, LOW);
      delay(5);
      attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonPress, LOW);
   }else if(mode == 1){
      
      analogWrite(RED, 100);
      analogWrite(GREEN, 100);
      analogWrite(BLUE, 0);

      if (hum > 42.0){
         analogWrite(RED, 0);
         analogWrite(GREEN, 150);
         analogWrite(BLUE, 0);
         detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
         delay(5);
         digitalWrite(RELAY_PIN, HIGH);
         delay(5);
         attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonPress, LOW);
      }else if((hum < 40.0) || (isnan(hum))){
         analogWrite(RED, 100);
         analogWrite(GREEN, 100);
         analogWrite(BLUE, 0);
         detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
         delay(5);
         digitalWrite(RELAY_PIN, LOW);
         delay(5);
         attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonPress, LOW);
      }
   }else if(mode == 2){
      analogWrite(RED, 100);
      analogWrite(GREEN, 0);
      analogWrite(BLUE, 200);
      detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
      delay(5);
      digitalWrite(RELAY_PIN, HIGH);
      delay(5);
      attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonPress, LOW);
   }
   
   if(millis() - humid_measure_time > 2000){
      hum = dht.readHumidity();
      humid_measure_time = millis();
   }
   
   wdt_reset();
}

void buttonPress()
{
   button_time = millis();
   
   if (button_time - last_button_time > 75){
      if(mode >= 2){
         mode = 0;
      }else{
         mode++;  
      }
   }
   last_button_time = button_time;
}   
