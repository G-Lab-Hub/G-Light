#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

// Definizione base dei colori
#define GC9A01A_BLACK   0x0000
#define GC9A01A_WHITE   0xFFFF
#define GC9A01A_RED     0xF800
#define GC9A01A_CYAN    0x07FF
#define GC9A01A_GREEN   0x07E0
#define GC9A01A_BLUE    0x001F

// Pin del display GC9A01 per Arduino collegato con SPI bus Hardware
#define TFT_SCL  13 // SCK / SCL 
#define TFT_SDA  11 // MOSI / SDA / DIN
#define TFT_CS    7 // CS (Chip Select)
#define TFT_DC    6 // DC (Data/Command)
#define TFT_RST   5 // RES / RST (Reset)
// Pin TFT_BL rimosso in quanto non utilizzato

// Pin del sensore di temperatura e della ventola
#define TEMP_SENSOR_PIN 2
#define PWM_PIN 3

// Pin per il controllo del mosfet e pulsanti
#define WHITE_BTN_PIN 14 // Pin digitale equivalente ad A0
#define BLUE_BTN_PIN 4   // Pulsante su D4 (retroilluminazione rimossa)
#define MOSFET_PIN 9     // Pin PWM per Arduino Nano

// Variabili per la gestione del led
int ledBrightness = 0;
int ledPct = 0;
bool ledState = false;
bool lastWhiteBtnState = false; // "false" significa NON premuto
bool lastBlueBtnState = HIGH;
unsigned long whiteBtnPressTime = 0;
bool whiteBtnHandledLong = false;
int dimDirection = 1;
unsigned long lastDebounceTimeWhite = 0;

// Variabili per l'interfaccia
bool isLedScreen = false;
bool forceScreenUpdate = false;

// Sensore temperatura non bloccante
unsigned long lastTempRequest = 0;
float currentTemp = 0.0;

// Inizializzazione display usando Hardware SPI
Adafruit_GC9A01A display = Adafruit_GC9A01A(TFT_CS, TFT_DC, TFT_RST);

// Inizializzazione del sensore di temperatura
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

const int ARC_START = 135;
const int ARC_END = 405;
int prev_angle = 135;

char oldTempStr[15] = "";
char oldFanStr[15] = "";
char oldLedStr[15] = "";
char oldBattStr[15] = "";
uint16_t oldTitleColor = 0xFFFF;

// --- COORDINATE DI POSIZIONAMENTO MANUALE ---
// X: posizione orizzontale (da 0 a 240, da sinistra a destra)
// Y: posizione verticale (da 0 a 240, dall'alto al basso)
// I font disegnano PARTENDO dal basso della lettera (la Y è la base/linea di base del testo)

int glight_X = 83;
int glight_Y = 75;

int main_val_X = 70;
int main_val_Y = 125;

int sub1_text_X = 110;
int sub1_text_Y = 158;
int sub1_icon_X = 95;
int sub1_icon_Y = 152;

int fan_text_X = 110;
int fan_text_Y = 186;
int fan_icon_X = 95;
int fan_icon_Y = 180;

int batt_text_X = 110;
int batt_text_Y = 215;
int batt_icon_X = 85;
int batt_icon_Y = 210;

// Calcola un gradiente di colore da Blu a Verde a Rosso in base al nuovo range (0-60 gradi)
uint16_t getTempColor(float temp) {
  uint8_t r = 0, g = 0, b = 0;
  // Freddo < 20C (tutto blu)
  if (temp <= 20.0) {
    b = 255;
  // Caldo > 45C (tutto rosso)
  } else if (temp >= 45.0) {
    r = 255;
  // Da Blu a Verde (20C - 32.5C)
  } else if (temp < 32.5) {
    float t = (temp - 20.0) / 12.5; 
    g = 255 * t;
    b = 255 * (1.0 - t);
  // Da Verde a Rosso (32.5C - 45C)
  } else {
    float t = (temp - 32.5) / 12.5;
    r = 255 * t;
    g = 255 * (1.0 - t);
  }
  return display.color565(r, g, b);
}

// Calcola un gradiente di colore da Blu a Verde a Rosso in base alla percentuale
uint16_t getLedColor(int pct) {
  uint8_t r = 0, g = 0, b = 0;
  if (pct == 0) {
    b = 255;
  } else if (pct <= 50) {
    float t = pct / 50.0;
    g = 255 * t;
    b = 255 * (1.0 - t);
  } else {
    float t = (pct - 50) / 50.0;
    r = 255 * t;
    g = 255 * (1.0 - t);
  }
  return display.color565(r, g, b);
}

void drawFanIcon(int x, int y, uint16_t color) {
  display.fillCircle(x, y, 1, color);
  display.fillCircle(x, y - 4, 3, color);
  display.fillCircle(x, y + 4, 3, color);
  display.fillCircle(x - 4, y, 3, color);
  display.fillCircle(x + 4, y, 3, color);
  display.drawCircle(x, y, 8, color);
}

void drawTorchIcon(int x, int y, uint16_t color) {
  // Corpo della torcia
  display.fillRect(x - 6, y - 2, 8, 5, color); 
  // Testa della torcia
  display.fillRect(x + 2, y - 4, 4, 9, color);
  // Raggi di luce
  display.drawLine(x + 7, y, x + 11, y, color);
  display.drawLine(x + 7, y - 3, x + 10, y - 6, color);
  display.drawLine(x + 7, y + 3, x + 10, y + 6, color);
}

void drawThermometerIcon(int x, int y, uint16_t color) {
  display.fillCircle(x, y + 4, 3, color);               // Bulbo
  display.drawRoundRect(x - 2, y - 6, 5, 10, 2, color); // Corpo
  display.drawLine(x, y - 4, x, y + 2, color);          // Linea interna
}

void drawBatteryIcon(int x, int y, uint16_t color) {
  display.drawRect(x, y - 5, 20, 10, color);
  display.fillRect(x + 20, y - 2, 3, 4, color);
  display.fillRect(x + 2, y - 3, 16, 6, color);
}

void updateArc(int new_angle, bool isLedMode) {
  uint16_t track_color = display.color565(30, 30, 30);
  
  if (new_angle > prev_angle) {
    for (float i = prev_angle; i <= new_angle; i += 0.2) {
      uint16_t color;
      if (isLedMode) {
        float ledVal = map((int)i, ARC_START, ARC_END, 0, 100);
        color = getLedColor((int)ledVal);
      } else {
        float tempVal = map((int)i, ARC_START, ARC_END, 100, 500) / 10.0;
        color = getTempColor(tempVal);
      }
      float rad = i * 0.0174533;
      float cos_val = cos(rad);
      float sin_val = sin(rad);
      for (int r = 90; r <= 118; r++) { 
        display.drawPixel(120 + r * cos_val, 120 + r * sin_val, color);
      }
    }
  } else if (new_angle < prev_angle) {
    for (float i = prev_angle; i >= new_angle; i -= 0.2) {
      float rad = i * 0.0174533;
      float cos_val = cos(rad);
      float sin_val = sin(rad);
      for (int r = 90; r <= 118; r++) {
        display.drawPixel(120 + r * cos_val, 120 + r * sin_val, track_color);
      }
    }
  }
  prev_angle = new_angle;
}

void printTextNoFlicker(const char* newText, const char* oldText, int x, int y, const GFXfont* font, uint16_t color) {
  display.setFont(font);
  int16_t x1, y1;
  uint16_t w, h;
  
  if (oldText[0] != '\0') {
    display.getTextBounds(oldText, x, y, &x1, &y1, &w, &h);
    display.fillRect(x1, y1, w + 2, h + 2, GC9A01A_BLACK);
  }
  
  display.setCursor(x, y);
  display.setTextColor(color);
  display.print(newText);
}

void setup() {
  // Inizializzazione pulsanti e mosfet
  // Rimuoviamo il PULLUP dal tasto bianco visto che funziona in modo invertito (forse hai una resistenza di pull-down?)
  pinMode(WHITE_BTN_PIN, INPUT);
  pinMode(BLUE_BTN_PIN, INPUT_PULLUP);
  pinMode(MOSFET_PIN, OUTPUT);
  analogWrite(MOSFET_PIN, 0);

  display.begin();
  display.setRotation(0);
  display.fillScreen(GC9A01A_BLACK);

  tempSensor.begin();
  tempSensor.setWaitForConversion(true);
  tempSensor.requestTemperatures();
  currentTemp = tempSensor.getTempCByIndex(0);
  if (currentTemp == DEVICE_DISCONNECTED_C || currentTemp == 85.0) currentTemp = 25.0; // fallback safe
  tempSensor.setWaitForConversion(false);
  tempSensor.requestTemperatures();
  lastTempRequest = millis();

  pinMode(PWM_PIN, OUTPUT);
  TCCR2B = (TCCR2B & 0b11111000) | 0x01;

  display.setFont(&FreeSansBold18pt7b);
  display.setTextColor(GC9A01A_CYAN);
  
  const char* bootTitle = "G-Light";
  int16_t xB, yB;
  uint16_t wB, hB;
  display.getTextBounds(bootTitle, 0, 0, &xB, &yB, &wB, &hB);
  display.setCursor(120 - (wB / 2), 125);
  display.print(bootTitle);

  uint16_t track_color = display.color565(30, 30, 30);
  for (float i = ARC_START; i <= ARC_END; i += 0.2) {
    float rad = i * 0.0174533;
    float cos_val = cos(rad);
    float sin_val = sin(rad);
    for (int r = 90; r <= 118; r++) { 
      display.drawPixel(120 + r * cos_val, 120 + r * sin_val, track_color);
    }
  }
  prev_angle = ARC_START;

  delay(1000);
  
  display.fillCircle(120, 120, 88, GC9A01A_BLACK);
}

void updateDisplay(float tempC, int fanPwm) {
  if (forceScreenUpdate) {
    display.fillCircle(120, 120, 88, GC9A01A_BLACK);
    oldTempStr[0] = '\0';
    oldLedStr[0] = '\0';
    oldFanStr[0] = '\0';
    oldBattStr[0] = '\0';
    oldTitleColor = 0xFFFF;
    forceScreenUpdate = false;
  }

  uint16_t primaryColor;
  int target_angle;
  char mainStr[15];
  char sub1Str[15];
  
  int displayLedPct = ledState ? ledPct : 0;

  if (isLedScreen) {
    target_angle = map(displayLedPct, 0, 100, ARC_START, ARC_END);
    primaryColor = getLedColor(displayLedPct);
    sprintf(mainStr, "%d %%", displayLedPct);
    dtostrf(tempC, 4, 1, sub1Str);
    strcat(sub1Str, " C");
  } else {
    target_angle = map(tempC * 10, 100, 500, ARC_START, ARC_END);
    primaryColor = getTempColor(tempC);
    dtostrf(tempC, 4, 1, mainStr);
    strcat(mainStr, " C");
    sprintf(sub1Str, "%d %%", displayLedPct);
  }
  target_angle = constrain(target_angle, ARC_START, ARC_END);

  if (primaryColor != oldTitleColor) {
    printTextNoFlicker(isLedScreen ? "Luce" : "G-Light", "", glight_X, glight_Y, &FreeSansBold9pt7b, primaryColor);
    oldTitleColor = primaryColor;
  }
  
  if (strcmp(mainStr, oldTempStr) != 0) { // Uso oldTempStr come buffer del valore principale
    printTextNoFlicker(mainStr, oldTempStr, main_val_X, main_val_Y, &FreeSansBold18pt7b, primaryColor);
    strcpy(oldTempStr, mainStr);
  }
  
  if (strcmp(sub1Str, oldLedStr) != 0) { // Uso oldLedStr come buffer del sub1
    uint16_t sub1Color = isLedScreen ? getTempColor(tempC) : getLedColor(displayLedPct);
    printTextNoFlicker(sub1Str, oldLedStr, sub1_text_X, sub1_text_Y, &FreeSansBold9pt7b, sub1Color); 
    
    display.fillRect(sub1_icon_X - 12, sub1_icon_Y - 12, 24, 24, GC9A01A_BLACK);
    if (isLedScreen) {
      drawThermometerIcon(sub1_icon_X, sub1_icon_Y, sub1Color);
    } else {
      drawTorchIcon(sub1_icon_X, sub1_icon_Y, sub1Color);
    }
    strcpy(oldLedStr, sub1Str);
  }

  char fanStr[15];
  int fanPct = (fanPwm * 100) / 255;
  sprintf(fanStr, "%d %%", fanPct);
  
  if (strcmp(fanStr, oldFanStr) != 0) {
    printTextNoFlicker(fanStr, oldFanStr, fan_text_X, fan_text_Y, &FreeSansBold9pt7b, GC9A01A_WHITE); 
    
    display.fillRect(fan_icon_X - 12, fan_icon_Y - 12, 24, 24, GC9A01A_BLACK);
    drawFanIcon(fan_icon_X, fan_icon_Y, GC9A01A_WHITE);
    
    strcpy(oldFanStr, fanStr);
  }

  char battStr[15] = "100%";
  if (strcmp(battStr, oldBattStr) != 0) {
    printTextNoFlicker(battStr, oldBattStr, batt_text_X, batt_text_Y, &FreeSansBold9pt7b, GC9A01A_GREEN);
    
    display.fillRect(batt_icon_X - 2, batt_icon_Y - 7, 30, 16, GC9A01A_BLACK);
    drawBatteryIcon(batt_icon_X, batt_icon_Y, GC9A01A_GREEN);
    
    strcpy(oldBattStr, battStr);
  }
  
  if (target_angle != prev_angle) {
    updateArc(target_angle, isLedScreen);
  }
}

void loop() {
  // --- Gestione Tasto Bianco (Corto: On/Off, Lungo: Dimmer) ---
  // Invertito il controllo: adesso assume che schiacciando dia HIGH
  bool readingWhite = (digitalRead(WHITE_BTN_PIN) == HIGH); 
  
  if (readingWhite && !lastWhiteBtnState) {
    // Fronte di salita (appena premuto)
    whiteBtnPressTime = millis();
    whiteBtnHandledLong = false;
  }
  
  if (readingWhite && lastWhiteBtnState) {
    // Mantenimento (tenendo premuto)
    if (millis() - whiteBtnPressTime > 400) { // Dopo 400ms inizia a dimmerare
      whiteBtnHandledLong = true;
      ledState = true;
      if (millis() - lastDebounceTimeWhite > 15) { // Velocità dimmeraggio
        ledPct += dimDirection;
        if (ledPct >= 100) {
          ledPct = 100;
          dimDirection = -5; // Inverti verso il basso
        } else if (ledPct <= 0) {
          ledPct = 0;
          dimDirection = 5; // Inverti verso l'alto
        }
        ledBrightness = (ledPct * 255) / 100; // Calcolo lineare
        analogWrite(MOSFET_PIN, ledBrightness);
        lastDebounceTimeWhite = millis();
      }
    }
  }
  
  if (!readingWhite && lastWhiteBtnState) {
    // Fronte di discesa (rilasciato)
    if (!whiteBtnHandledLong && (millis() - whiteBtnPressTime > 50)) {
      // È stato un click corto
      ledState = !ledState;
      if (ledState && ledPct == 0) {
        ledPct = 100;
      }
      ledBrightness = ledState ? ((ledPct * 255) / 100) : 0;
      analogWrite(MOSFET_PIN, ledBrightness);
      if (!ledState) {
        // Se spegniamo e riaccendiamo, la direzione del dimmer parte dall'alto in basso o da dove era?
        // Non critico, lasciamola dov'era.
      }
    }
  }
  lastWhiteBtnState = readingWhite;

  // --- Gestione Tasto Blu (Cambia Schermata) ---
  bool readingBlue = digitalRead(BLUE_BTN_PIN);
  if (readingBlue != lastBlueBtnState) {
    if (readingBlue == LOW) { // Premuto
      isLedScreen = !isLedScreen;
      forceScreenUpdate = true;
    }
    delay(50); // Debounce brutale per il blu
  }
  lastBlueBtnState = readingBlue;

  // --- Gestione Sensore e Ventola ---
  if (millis() - lastTempRequest > 1000) { // Timeout un po' più lungo per sicurezze hw
    float temp = tempSensor.getTempCByIndex(0);
    if (temp != DEVICE_DISCONNECTED_C && temp != 85.0 && temp > -50.0) {
      currentTemp = temp;
    }
    // Avvia la successiva lettura in modo asincrono
    tempSensor.setWaitForConversion(false);
    tempSensor.requestTemperatures();
    lastTempRequest = millis();
  }

  int fanSpeed = map(currentTemp, 25, 40, 0, 255);
  fanSpeed = constrain(fanSpeed, 0, 255);
  analogWrite(PWM_PIN, fanSpeed);

  updateDisplay(currentTemp, fanSpeed);

  delay(20); // Delay abbassato per permettere un dimmer fluido
}
