#include "globals.h"

// DHT 객체 생성
DHT dht(DHTPIN, DHTTYPE);

void setup_dht() {
  dht.begin();
  // 초기화 메시지는 디버깅용으로 둬도 됨
  Serial.println("DHT22 Initialized.");
}

void read_dht_data() {
  // 값 읽기
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  // 값이 정상(숫자)일 때만 전역 변수 업데이트
  // isnan()은 "숫자가 아님(Error)"을 감지하는 함수
  if (!isnan(t)) {
    curTemp = t;
  } 
  // 에러일 때는 기존 값을 유지하거나, 필요하면 디버깅 메시지만 출력
  // else { Serial.println("DHT Temp Error"); }

  if (!isnan(h)) {
    curHum = h;
  } 
  // else { Serial.println("DHT Hum Error"); }

  // [삭제됨] JSON 출력 코드는 여기서 삭제했습니다! 
  // 메인 파일(smart_home.ino)에서 한 번만 보내야 깔끔합니다.
}