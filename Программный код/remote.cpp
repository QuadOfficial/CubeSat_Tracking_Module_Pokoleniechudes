#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define PIN_CE    9
#define PIN_CSN   10
#define PIN_BTN   2
#define PIN_BUZZ  A2
#define PIN_JOY_H A1
#define PIN_JOY_V A0

RF24 radio(PIN_CE, PIN_CSN);
const uint64_t pipeCmd = 0xE8E8F0F0E1LL;
const uint64_t pipeTlm = 0xE8E8F0F0E2LL;

Adafruit_SSD1306 display(128, 64, &Wire, -1);

struct CmdPack {
  byte id;
  byte joyX;
  byte joyY;
  byte cmd;
};

struct TlmPack {
  byte id;
  int8_t angH;
  int8_t angV;
  byte mode;
  byte laser;
};

CmdPack cmd;
TlmPack tlm;

bool laserState = false;
bool lastBtn = false;
unsigned long pressT = 0;
bool inAutoMode = false;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_BUZZ, OUTPUT);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(15, 5);
  display.println(F("CUBESAT"));
  display.setCursor(15, 30);
  display.println(F("PULT OK"));
  display.display();
  delay(1000);

  radio.begin();
  radio.setChannel(5);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setAutoAck(true);
  radio.setRetries(5, 15);
  radio.openWritingPipe(pipeCmd);
  radio.openReadingPipe(1, pipeTlm);
  radio.stopListening();

  cmd.id = 0x42;
  cmd.cmd = 0;

  tone(PIN_BUZZ, 2000, 100);
}

void loop() {
  if (inAutoMode) {
    loopAutoDisplay();
  } else {
    loopManual();
  }
}

void loopManual() {
  cmd.joyX = map(analogRead(PIN_JOY_H), 0, 1023, 255, 0);
  cmd.joyY = map(analogRead(PIN_JOY_V), 0, 1023, 0, 255);

  bool btn = !digitalRead(PIN_BTN);

  if (btn && !lastBtn) {
    pressT = millis();
    lastBtn = true;
  }

  if (btn && lastBtn && (millis() - pressT > 2000)) {
    cmd.cmd = 3;
    radio.stopListening();
    radio.write(&cmd, sizeof(cmd));
    cmd.cmd = 0;
    lastBtn = false;

    tone(PIN_BUZZ, 1000, 500);

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 5);
    display.println(F("AUTO"));
    display.setCursor(10, 30);
    display.println(F("START!"));
    display.display();
    delay(1500);

    inAutoMode = true;
    radio.startListening();
    return;
  }

  if (!btn && lastBtn) {
    if (millis() - pressT < 2000) {
      laserState = !laserState;
      tone(PIN_BUZZ, 4000, 50);
    }
    lastBtn = false;
  }

  cmd.cmd = laserState ? 1 : 2;

  radio.stopListening();
  radio.write(&cmd, sizeof(cmd));

  radio.startListening();
  unsigned long startWait = micros();
  while (micros() - startWait < 5000) { 
    if (radio.available()) {
      radio.read(&tlm, sizeof(tlm));
      break;
    }
  }
  radio.stopListening();

  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(0, 0);
  if (laserState) display.print(F("LASER: ON"));
  else display.print(F("LASER:OFF"));

  display.drawRect(0, 20, 128, 8, WHITE);
  display.fillRect(0, 20, map(cmd.joyX, 0, 255, 0, 128), 8, WHITE);

  display.drawRect(0, 32, 128, 8, WHITE);
  display.fillRect(0, 32, map(cmd.joyY, 0, 255, 0, 128), 8, WHITE);

  display.setTextSize(2);
  display.setCursor(0, 48);
  display.print(F("H:"));
  display.print(tlm.angH);

  display.setCursor(68, 48);
  display.print(F("V:"));
  display.print(tlm.angV);

  display.display();
  delay(10);
}

void loopAutoDisplay() {
  if (radio.available()) {
    radio.read(&tlm, sizeof(tlm));

    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(0, 0);
    switch (tlm.mode) {
      case 2:  display.print(F("HOR SCAN")); break;
      case 3:  display.print(F("VER SCAN")); break;
      case 4:  display.print(F("DIAG 1"));   break;
      case 5:  display.print(F("DIAG 2"));   break;
      case 0:  display.print(F("DONE!"));    break;
      default: display.print(F("MODE:")); display.print(tlm.mode); break;
    }

    display.setTextSize(3);
    display.setCursor(0, 18);
    if (tlm.angH >= 0) display.print(F("+"));
    display.print(tlm.angH);
    display.print(F(" "));
    if (tlm.angV >= 0) display.print(F("+"));
    display.print(tlm.angV);

    display.setTextSize(2);
    display.setCursor(0, 48);
    if (tlm.laser) display.print(F("LASER ON"));
    else display.print(F("LASER OF"));

    display.display();

    if (tlm.mode == 0) {
      delay(500);
      tone(PIN_BUZZ, 3000, 300);
      delay(300);
      tone(PIN_BUZZ, 3000, 300);
      inAutoMode = false;
      radio.stopListening();
    }
  }
  delay(10);
}