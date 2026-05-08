#include <ESP32Servo.h>
Servo myServo;

const int servoPin = 23;
int angle = 0;
int step = 15;        
int delayTime = 1; 

void setup() {
  myServo.attach(servoPin);
}

void loop() {
  
  for (angle = 0; angle <= 180; angle += step) {
    myServo.write(angle);
    delay(delayTime);
  }

  delay(500);

  for (angle = 180; angle >= 0; angle -= step) {
    myServo.write(angle);
    delay(delayTime);
  }

  delay(500);
}
