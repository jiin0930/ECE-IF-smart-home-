// co2.cpp
#include "globals.h"

// 통신 객체 생성
SoftwareSerial co2Serial(CO2_RX_PIN, CO2_TX_PIN);

// 명령어 및 버퍼
byte co2Cmd[4] = {0x11, 0x01, 0x01, 0xED};
unsigned char co2Response[8];

void setup_co2() {
  co2Serial.begin(9600);
  // Serial.println("CO2 Sensor Setup Done.");
}

void read_co2() {
  // 1. 버퍼 비우기
  while (co2Serial.available() > 0) co2Serial.read();

  // 2. 요청 전송
  for (int i = 0; i < 4; i++) co2Serial.write(co2Cmd[i]);

  // 3. 대기
  unsigned long start = millis();
  while (co2Serial.available() < 8) {
    if (millis() - start > 1000) return; // 타임아웃
  }

  // 4. 읽기
  for (int i = 0; i < 8; i++) co2Response[i] = co2Serial.read();

  // 5. 값 갱신 (전역 변수 curCO2 업데이트!)
  if (co2Response[0] == 0x16) {
    curCO2 = co2Response[3] * 256 + co2Response[4];
  }
} 
