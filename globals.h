#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <SoftwareSerial.h>
#include <DHT.h>

// --- 핀 설정 ---
#define HUM_PIN    9   
#define FAN_PIN    10  
#define HEAT_PIN   11  

#define CO2_RX_PIN 4 
#define CO2_TX_PIN 3

#define DHTPIN     2   
#define DHTTYPE    DHT22

#define WIN_SERVO_START_CH  0  // 첫 번째 서보모터가 꽂힌 번호 (0번)
#define WIN_SERVO_COUNT     4  // 창문(서보모터) 개수 (4개)

// --- 객체 공유 ---
extern Adafruit_PWMServoDriver pca;
extern SoftwareSerial co2Serial;
extern DHT dht;

// --- 변수 공유 (모든 센서값은 여기에 저장) ---
extern int curCO2;    
extern float curTemp;
extern float curHum;

// --- 함수 선언 ---
void setup_co2();
void read_co2();

void setup_dht();
void read_dht_data(); // 온습도를 한 번에 읽는 함수

void setWindowAngle(int angle);

#endif