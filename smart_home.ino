#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "globals.h" 

// ===== 하드웨어 핀 설정 =====
#define LED_PIN          6           
#define LIGHT_SENSOR_PIN A1         

// 객체 생성
Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(0x40);

// 변수들
int curCO2 = 0;
float curTemp = 0.0;
float curHum = 0.0;
int curLight = 0; 

#define SERVOMIN  150
#define SERVOMAX  600

enum SystemMode { MANUAL, AUTO_HOME, AUTO_AWAY, AUTO_SLEEP };
SystemMode currentMode = AUTO_HOME;

unsigned long lastSensorRead = 0;
const long sensorInterval = 2000; 

unsigned long lastLedUpdate = 0;
const long ledInterval = 50;       

bool isWindowOpen = false;
float currentPWM = 0; 

// 제어 신호 버퍼
const int BUFFER_MAX = 1; 
int cntHeat = 0;
int cntHum = 0;
int cntAir = 0;

// JSON 수신 버퍼
String jsonBuffer = "";
bool isReceiving = false;
unsigned long lastCharTime = 0;
const unsigned long TIMEOUT_MS = 10000; // 10초로 변경

void setup() {
  Serial.begin(9600);
  
  setup_co2();
  setup_dht();

  pinMode(HUM_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(HEAT_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LIGHT_SENSOR_PIN, INPUT);

  pca.begin();
  pca.setPWMFreq(60);
  
  digitalWrite(HUM_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(HEAT_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  setWindowAngle(0);

  Serial.println("=== Smart Home System Ready ===");
}

void loop() {
  handleSerialInput(); 

  unsigned long currentMillis = millis();

  if (currentMillis - lastSensorRead >= sensorInterval) {
    lastSensorRead = currentMillis;
    readSensors();      
    runControlLogic(); 
    sendSensorDataJSON(); 
  }

  if (currentMillis - lastLedUpdate >= ledInterval) {
    lastLedUpdate = currentMillis;
    if (currentMode != MANUAL) {
      controlSmartLED_Smooth(); 
    } else {
      currentPWM = 0;
    }
  }
}

// ===== JSON 수신 처리 =====
void handleSerialInput() {
  unsigned long currentTime = millis();
  
  if (isReceiving && (currentTime - lastCharTime > TIMEOUT_MS)) {
    Serial.print(">> [TIMEOUT] Buffer: ");
    Serial.println(jsonBuffer);
    jsonBuffer = "";
    isReceiving = false;
  }
  
  while (Serial.available() > 0) {
    char c = Serial.read();
    lastCharTime = currentTime;
    
    if (c == '{') {
      isReceiving = true;
      jsonBuffer = "{";
      Serial.println("\n>> [START] JSON reception");
    }
    else if (c == '}') {
      if (isReceiving) {
        jsonBuffer += "}";
        
        Serial.println("========================================");
        Serial.print(">> [BUFFER] Size: ");
        Serial.println(jsonBuffer.length());
        Serial.print(">> [BUFFER] Raw: ");
        Serial.println(jsonBuffer);
        
        parseControlJSON(jsonBuffer);
        
        jsonBuffer = "";
        isReceiving = false;
        
        Serial.println(">> [DONE] Buffer cleared\n");
      }
    }
    else {
      if (isReceiving) {
        jsonBuffer += c;
        
        if (jsonBuffer.length() > 300) {
          Serial.println(">> [ERROR] Buffer overflow");
          jsonBuffer = "";
          isReceiving = false;
        }
      }
    }
  }
}

void resetBuffers() {
    cntHeat = 0;
    cntHum = 0;
    cntAir = 0;
}

// ===== JSON 파싱 (공백/개행 제거) =====
void parseControlJSON(String json) {
  Serial.println(">> [PARSE] Starting...");
  
  // 공백, 개행 모두 제거
  json.replace(" ", "");
  json.replace("\n", "");
  json.replace("\r", "");
  json.replace("\t", "");
  
  Serial.print(">> [CLEAN] ");
  Serial.println(json);
  
  // 모드 파싱
  int modePos = json.indexOf("\"mode\":");
  if (modePos != -1) {
    if (json.indexOf("\"sudong\"") >= 0) {
      currentMode = MANUAL; resetBuffers(); Serial.println(">> [MODE] MANUAL (sudong)");
    }
    else if (json.indexOf("\"in\"") >= 0) {
      currentMode = AUTO_HOME; resetBuffers(); Serial.println(">> [MODE] AUTO_HOME (in)");
    }
    else if (json.indexOf("\"out\"") >= 0) {
      currentMode = AUTO_AWAY; resetBuffers(); Serial.println(">> [MODE] AUTO_AWAY (out)");
    }
    else if (json.indexOf("\"zzz\"") >= 0) {
      currentMode = AUTO_SLEEP; resetBuffers(); Serial.println(">> [MODE] AUTO_SLEEP (zzz)");
    }
  }

  if (currentMode != MANUAL) {
    Serial.println(">> [SKIP] Not manual mode");
    return;
  }

  // 창문 (win)
  int wPos = json.indexOf("\"win\":");
  if (wPos != -1) {
    int val = json.charAt(wPos + 6) - '0';
    if (val == 1) { 
      setWindowAngle(90); 
      isWindowOpen = true; 
      Serial.println(">> [WIN] OPEN"); 
    }
    else if (val == 0) { 
      setWindowAngle(0); 
      isWindowOpen = false; 
      Serial.println(">> [WIN] CLOSED"); 
    }
  }

  // 팬 (fan)
  int fanPos = json.indexOf("\"fan\":");
  if (fanPos != -1) {
    int val = json.charAt(fanPos + 6) - '0';
    if (val == 1) { 
      digitalWrite(FAN_PIN, HIGH);
      Serial.print(">> [FAN] ON (Pin");
      Serial.print(FAN_PIN);
      Serial.println(")");
    }
    else if (val == 0) { 
      digitalWrite(FAN_PIN, LOW);
      Serial.print(">> [FAN] OFF (Pin");
      Serial.print(FAN_PIN);
      Serial.println(")");
    }
  } else {
    Serial.println(">> [FAN] Not found in JSON");
  }

  // 조명 (glight)
  int glightPos = json.indexOf("\"glight\":");
  if (glightPos != -1) {
    int val = json.charAt(glightPos + 9) - '0';
    if (val == 1) {
      analogWrite(LED_PIN, 255);
      currentPWM = 255;
      Serial.println(">> [GLIGHT] ON"); 
    }
    else if (val == 0) { 
      analogWrite(LED_PIN, 0);
      currentPWM = 0;
      Serial.println(">> [GLIGHT] OFF"); 
    }
  }

  // 히터 (hit)
  int hitPos = json.indexOf("\"hit\":");
  if (hitPos != -1) {
    int val = json.charAt(hitPos + 6) - '0';
    if (val == 1) {
      digitalWrite(HEAT_PIN, HIGH);
      Serial.print(">> [HIT] ON (Pin");
      Serial.print(HEAT_PIN);
      Serial.println(")");
    }
    else if (val == 0) {
      digitalWrite(HEAT_PIN, LOW);
      Serial.print(">> [HIT] OFF (Pin");
      Serial.print(HEAT_PIN);
      Serial.println(")");
    }
  }

  // 가습기 (hum)
  int humPos = json.indexOf("\"hum\":");
  if (humPos != -1) {
    int val = json.charAt(humPos + 6) - '0';
    if (val == 1) {
      digitalWrite(HUM_PIN, HIGH);
      Serial.print(">> [HUM] ON (Pin");
      Serial.print(HUM_PIN);
      Serial.println(")");
    }
    else if (val == 0) {
      digitalWrite(HUM_PIN, LOW);
      Serial.print(">> [HUM] OFF (Pin");
      Serial.print(HUM_PIN);
      Serial.println(")");
    }
  }
}

void readSensors() {
  read_co2();
  delay(50); 
  read_dht_data();
  curLight = analogRead(LIGHT_SENSOR_PIN);
}

void controlSmartLED_Smooth() {
  int sensorVal = analogRead(LIGHT_SENSOR_PIN); 
  int targetPWM = 0;
  if (sensorVal < 400) targetPWM = 0; 
  else {
    targetPWM = map(sensorVal, 400, 1023, 0, 255);
    targetPWM = constrain(targetPWM, 0, 255);
  }
  currentPWM = (currentPWM * 0.9) + (targetPWM * 0.1);
  analogWrite(LED_PIN, (int)currentPWM);
}

void sendSensorDataJSON() {
  Serial.print("{\"temp\":");
  Serial.print(curTemp, 1); 
  Serial.print(",\"humi\":");
  Serial.print(curHum, 1); 
  Serial.print(",\"co2\":");
  Serial.print(curCO2); 
  Serial.print(",\"light\":");
  Serial.print(curLight);
  Serial.println("}");
}

void setWindowAngle(int angle) {
  int pulse = map(angle, 0, 180, SERVOMIN, SERVOMAX);
  
  // 0번 채널 작동
  pca.setPWM(0, 0, pulse);   
  delay(100);  // 0.1초 대기 (전류 분산)

  // 4번 채널 작동
  pca.setPWM(4, 0, pulse);   
  delay(100);

  // 8번 채널 작동
  pca.setPWM(8, 0, pulse);   
  delay(100);

  // 12번 채널 작동
  pca.setPWM(12, 0, pulse);  
  
  // 로그 출력
  Serial.print(">> [SERVO] All channels moved to ");
  Serial.print(angle);
  Serial.println(" deg (Sequential)");
}

void runControlLogic() {
  if (currentMode == MANUAL) return;
  switch (currentMode) {
    case AUTO_HOME:   controlDevice(25, 26, 40, 60, 1000); break; //controlDevice(22, 26, 40, 60, 1000); break;
    case AUTO_AWAY:   controlDevice(10, 15, 20, 40, 2000); break;
    case AUTO_SLEEP:  controlDevice(22, 24, 50, 60, 800); break;
  }
}

void controlDevice(int minT, int maxT, int minH, int maxH, int maxCO2) {
  // [1] 온도 제어 (히터) - 즉시 반응
  if (curTemp < minT) {
    digitalWrite(HEAT_PIN, HIGH);
  } 
  else if (curTemp > maxT) {
    digitalWrite(HEAT_PIN, LOW);
  }

  // [2] 습도 제어 (가습기) - 즉시 반응
  if (curHum < minH) {
    digitalWrite(HUM_PIN, HIGH);
  } 
  else if (curHum > maxH) {
    digitalWrite(HUM_PIN, LOW);
  }

  // [3] 공기질 제어 (창문 & 팬) - 즉시 반응
  // CO2가 기준치보다 높으면 환기 시작
  if (curCO2 > maxCO2) {
     if (!isWindowOpen) {
       setWindowAngle(90); 
       delay(500); // 창문 열리는 시간 대기
       digitalWrite(FAN_PIN, HIGH);
       isWindowOpen = true;
       Serial.println(">> [AUTO] 환기 시작 (CO2 초과)");
     }
  } 
  // CO2가 충분히 낮아지면 환기 종료 (기준치 - 200)
  else if (curCO2 < (maxCO2 - 200)) {
     if (isWindowOpen) {
       digitalWrite(FAN_PIN, LOW); 
       delay(500); // 팬 멈추는 시간 대기
       setWindowAngle(0);
       isWindowOpen = false;
       Serial.println(">> [AUTO] 환기 종료 (CO2 정상)");
     }
  }
}