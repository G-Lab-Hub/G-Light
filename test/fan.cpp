#include <Arduino.h>
const int pwmPin = 3;  // Pin PWM collegato al Gate del MOSFET

void setup() {
  pinMode(pwmPin, OUTPUT);

  // Configura Timer2 per generare PWM ad alta frequenza (~31 kHz) su pin 3
  // Questo è necessario per far interpretare correttamente il duty cycle alla ventola
  TCCR2B = (TCCR2B & 0b11111000) | 0x01;  // prescaler = 1
}

void loop() {
  // Massima velocità
  analogWrite(pwmPin, 255);  // 100% duty cycle
  delay(5000);

  // Velocità media
  analogWrite(pwmPin, 128);  // 50% duty cycle
  delay(5000);

  // Fermare la ventola (alcune ventole si fermano solo se l'alimentazione è tolta)
  analogWrite(pwmPin, 1);    // 0% duty cycle
  delay(5000);
}