#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Servo.h>

const byte DEVICE_ID = 0x42;
// Калибровка под ваше устройство (H: -2, V: +17)
const int OFFSET_H = -2;   
const int OFFSET_V = 17;   

const int SERVO_CENTER = 90;
// Увеличен коэффициент для компенсации малого расстояния (было 1.0, стало 1.2)
const float SERVO_SCALE = 1.2; 

#define PIN_CE      7
#define PIN_CSN     8
#define PIN_SERVO_H 9
#define PIN_SERVO_V 10
#define PIN_LASER   4
#define PIN_BUZZ    6

RF24 radio(PIN_CE, PIN_CSN);
const uint64_t pipeCmd = 0xE8E8F0F0E1LL;
const uint64_t pipeTlm = 0xE8E8F0F0E2LL;

Servo servoH;
Servo servoV;

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

float manH = 0.0;
float manV = 0.0;
bool laserOn = false;

const int8_t SCAN[] = {-40, -30, -20, -10, 0, 10, 20, 30, 40};
const int SCAN_N = 9;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LASER, OUTPUT);
  pinMode(PIN_BUZZ, OUTPUT);

  servoH.attach(PIN_SERVO_H);
  servoV.attach(PIN_SERVO_V);
  setAng(0, 0);

  radio.begin();
  radio.setChannel(5);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setAutoAck(true);
  radio.setRetries(5, 15);
  radio.openReadingPipe(1, pipeCmd);
  radio.openWritingPipe(pipeTlm);
  radio.startListening();

  tlm.id = DEVICE_ID;
  tone(PIN_BUZZ, 2000, 200);
}

void loop() {
  if (!radio.available()) return;

  radio.read(&cmd, sizeof(cmd));

  switch (cmd.cmd) {
    case 1: 
      laserOn = true;
      digitalWrite(PIN_LASER, HIGH);
      break;
    case 2: 
      laserOn = false;
      digitalWrite(PIN_LASER, LOW);
      break;
    case 3: 
      runAuto();
      return;
  }

  if (cmd.joyX > 140) manH -= 1.0;
  if (cmd.joyX < 115) manH += 1.0;
  if (cmd.joyY > 140) manV += 1.0;
  if (cmd.joyY < 115) manV -= 1.0;

  manH = constrain(manH, -40, 40);
  manV = constrain(manV, -40, 40);
  setAng((int)manH, (int)manV);

  tlm.angH = (int8_t)manH;
  tlm.angV = (int8_t)manV;
  tlm.mode = 1;
  tlm.laser = laserOn ? 1 : 0;
  
  radio.stopListening();
  radio.write(&tlm, sizeof(tlm));
  radio.startListening();
}

void setAng(int h, int v) {
  int sH = SERVO_CENTER + (int)(h * SERVO_SCALE) + OFFSET_H;
  int sV = SERVO_CENTER + (int)(v * SERVO_SCALE) + OFFSET_V;
  
  // Расширенные лимиты для увеличенного SCALE
  sH = constrain(sH, 5, 175);
  sV = constrain(sV, 5, 175);
  
  servoH.write(sH);
  servoV.write(sV);
}

void runAuto() {
  tone(PIN_BUZZ, 1000, 500);
  digitalWrite(PIN_LASER, HIGH);
  laserOn = true;

  setAng(0, 0);
  tlm.angH = 0; tlm.angV = 0; tlm.mode = 0; tlm.laser = 1;
  
  radio.stopListening();
  radio.write(&tlm, sizeof(tlm));
  radio.startListening();
  
  delay(2000);

  for (int i = 0; i < SCAN_N; i++) {
    doStep(0, SCAN[i], 2);
  }
  setAng(0, 0);
  delay(500);

  for (int i = 0; i < SCAN_N; i++) {
    doStep(SCAN[i], 0, 3);
  }
  setAng(0, 0);
  delay(500);

  for (int i = 0; i < SCAN_N; i++) {
    doStep(SCAN[i], SCAN[i], 4);
  }
  setAng(0, 0);
  delay(500);

  for (int i = 0; i < SCAN_N; i++) {
    doStep(SCAN[i], SCAN[SCAN_N - 1 - i], 5);
  }

  setAng(0, 0);
  digitalWrite(PIN_LASER, LOW);
  laserOn = false;

  tlm.angH = 0; tlm.angV = 0; tlm.mode = 0; tlm.laser = 0;
  radio.stopListening();
  radio.write(&tlm, sizeof(tlm));
  radio.startListening();

  tone(PIN_BUZZ, 3000, 100);
  delay(150);
  tone(PIN_BUZZ, 3000, 100);
  
  manH = 0; manV = 0;
}

void doStep(int h, int v, byte mode) {
  setAng(h, v);
  tone(PIN_BUZZ, 4000, 30);

  tlm.angH = (int8_t)h;
  tlm.angV = (int8_t)v;
  tlm.mode = mode;
  tlm.laser = 1;
  
  radio.stopListening();
  radio.write(&tlm, sizeof(tlm));
  radio.startListening();

  delay(3000);
}