#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Pin del display Nokia 5110
#define RST_PIN 9
#define CE_PIN 8
#define DC_PIN 7
#define DIN_PIN 6
#define CLK_PIN 5
#define BL_PIN 4

// Pin del sensore di temperatura e della ventola
#define TEMP_SENSOR_PIN 2
#define PWM_PIN 3

// Inizializzazione del display
Adafruit_PCD8544 display = Adafruit_PCD8544(CLK_PIN, DIN_PIN, DC_PIN, CE_PIN, RST_PIN);

// Inizializzazione del sensore di temperatura
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

void setup() {
  // Configura il pin della retroilluminazione come output
  pinMode(BL_PIN, OUTPUT);
  digitalWrite(BL_PIN, HIGH); // Accendi la retroilluminazione

  // Inizializza il display
  display.begin();
  display.setContrast(14); // Regola il contrasto (valore tra 0 e 127)
  display.clearDisplay();

  // Inizializza il sensore di temperatura
  tempSensor.begin();

  // Configura il pin della ventola come output
  pinMode(PWM_PIN, OUTPUT);

  // Configura Timer2 per generare PWM ad alta frequenza (~31 kHz) su pin 3
  TCCR2B = (TCCR2B & 0b11111000) | 0x01; // prescaler = 1

  // Mostra un messaggio di test
  display.setTextSize(1.3);
  display.setTextColor(BLACK);
  display.setCursor(10, 10);
  display.println("Test Display");
  display.println("Nokia 5110");
  display.display();
  digitalWrite(BL_PIN, HIGH);
}

void loop() {
  // Richiedi la temperatura dal sensore
  tempSensor.requestTemperatures();
  float temperatureC = tempSensor.getTempCByIndex(0);

  // Calcola la velocità della ventola in base alla temperatura
  int fanSpeed = map(temperatureC, 25, 40, 0, 255); // Mappa 25-40°C a 0-100% PWM
  fanSpeed = constrain(fanSpeed, 0, 255); // Limita il valore tra 0 e 255
  analogWrite(PWM_PIN, fanSpeed);

  // Aggiorna il display
  display.clearDisplay();

  // Disegna la linea orizzontale
  display.drawLine(0, 27, 83, 27, BLACK);

  // Mostra la temperatura nella parte superiore
  display.setCursor(0, 10); // Evita le prime 3 righe
  display.setTextSize(1);
  display.print("T: ");
  display.print(temperatureC, 1);
  display.print("C");

  // Mostra la percentuale della ventola nella parte inferiore
  display.setCursor(0, 35); // Parte inferiore del display
  display.setTextSize(1);
  display.print("fan: ");
  display.print((fanSpeed / 255.0) * 100, 0);
  display.print("%");

  display.display();

  delay(100); // Aggiorna ogni 100 ms
}
