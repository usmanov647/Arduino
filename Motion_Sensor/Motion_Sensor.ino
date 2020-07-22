
#define MOTION_TIMER 60ul
#define SECONDS_CONST 1000ul //Seconds constant

unsigned long button_time = 0;
unsigned long last_button_time = 0;
unsigned long motion_time = 0;

int mode = 1;
int red = 10;
int green = 11;
int blue = 3;
int button_pin = 2;
int motion_pin = 12;
int relay_pin = 13;
int motion;
bool in_motion = true;

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin 13 as an output.
   pinMode(button_pin, INPUT);
   digitalWrite(button_pin, HIGH);
   
   pinMode(red, OUTPUT);
   pinMode(green, OUTPUT);
   pinMode(blue, OUTPUT);
   pinMode(relay_pin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(button_pin), buttonPress, LOW);
}
     
// the loop function runs over and over again forever
void loop() {

   if(mode == 0){
      analogWrite(red, 0);
      analogWrite(green, 0);
      analogWrite(blue, 150);
      detachInterrupt(digitalPinToInterrupt(button_pin));
      delay(5);
      digitalWrite(relay_pin, HIGH);
      delay(5);
      attachInterrupt(digitalPinToInterrupt(button_pin), buttonPress, LOW);
   }else if(mode == 1){
      if(in_motion){
         analogWrite(red, 0);
         analogWrite(green, 150);
         analogWrite(blue, 0);
         detachInterrupt(digitalPinToInterrupt(button_pin));
         delay(5);
         digitalWrite(relay_pin, LOW);
         delay(5);
         attachInterrupt(digitalPinToInterrupt(button_pin), buttonPress, LOW);
      }else{
         analogWrite(red, 100);
         analogWrite(green, 100);
         analogWrite(blue, 0);
         detachInterrupt(digitalPinToInterrupt(button_pin));
         delay(5);
         digitalWrite(relay_pin, HIGH);
         delay(5);
         attachInterrupt(digitalPinToInterrupt(button_pin), buttonPress, LOW);
      }
   }else if(mode == 2){
      analogWrite(red, 100);
      analogWrite(green, 0);
      analogWrite(blue, 200);
      detachInterrupt(digitalPinToInterrupt(button_pin));
      delay(5);
      digitalWrite(relay_pin, LOW);
      delay(5);
      attachInterrupt(digitalPinToInterrupt(button_pin), buttonPress, LOW);
   }

   motion = digitalRead(motion_pin);
   if(motion){
      motion_time = millis();
   }

   if(millis()-motion_time > (MOTION_TIMER*SECONDS_CONST)){
      in_motion = false;
   }else{
      in_motion = true;
   }
   
}

void buttonPress()
{
   button_time = millis();
   
   if (button_time - last_button_time > 75)
   {
      if(mode >= 2)
      {
         mode = 0;
      }else {
         mode++;  
      }
   }
   last_button_time = button_time;
}
