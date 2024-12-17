#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2); // LCD 초기화 (I2C 주소: 0x27, 화면 크기: 16x2)

// RFID 설정
#define RST_PIN  5  // Reset
#define SS_PIN  53  // slave Select
MFRC522 mfrc522(SS_PIN, RST_PIN); // RFID 인스턴스 생성

// servo motor 설정
Servo myservo; // 객체 생성
int pos = 0; // 서보모터 각도
int servoPin = 6; // 서보모터 제어 핀

// Wi-fi 설정
String ssid = "jeongtong"; // Wi-fi 이름
String PASSWORD = "12345678"; // Wi-fi 비밀번호
String host = "192.168.0.75"; // 서버 주소
String port = "9090"; // 서버 포트 번호

// Wi-fi 연결 함수
void connectWifi(){

  Serial.println(Serial3.read()); // ESP8266에 남아있는 데이터 출력
  String join ="AT+CWJAP=\""+ssid+"\",\""+PASSWORD+"\""; // Wi-fi 연결 명령 생성

  Serial.println("Connect Wifi...");
  delay(1000);
  Serial3.println(join); // ESP8266에 Wi-fi 연결 명령 전송
  delay(3000);

  if(Serial3.find("OK")) // Wi-fi 연결 성공 시
  {
    Serial.print("WIFI connect\n"); // 연결 성공 시 메시지 출력
  }else
  {
    Serial.println("connect timeout\n"); // 연결 실패 시 메시지 출력
  }
  delay(1000);
}

// 서버와 TCP 연결 함수
void connectTCP(){
    delay(100);
    Serial.println("connect TCP...");
    Serial3.println("AT+CIPSTART=\"TCP\",\""+host+"\","+port); // ESP8266에 TCP 연결 명령 전송
    delay(100);
    if(Serial.find("ERROR")) return; // 연결 실패 시 함수 종료
}

// 클라이언트 -> 서버 데이터 전송 함수
void httpclient(String char_input){
  String cmd=char_input; // 전송할 데이터
  Serial3.print("AT+CIPSEND="); // 데이터 길이 전송 명령
  Serial3.println(cmd.length()); // 전송 데이터 길이 입력
  Serial.print("AT+CIPSEND=");
  Serial.println(cmd.length());

  if(Serial3.find(">")) // ESP8266 데이터 전송 준비 상태 확인
  {
    Serial.print(">"); // 전송 가능
  }else
  {
    Serial.println("connect timeout"); // 전송 불가능
    delay(100);
    Serial3.println("AT+CIPCLOSE"); // 서버 연결 종료
    connectWifi(); // Wi-fi 재연결
    connectTCP(); // TCP 재연결
  }

  delay(50);
  Serial3.println(cmd); // 데이터 전송
  Serial.println(cmd);
  delay(100);
}  

// 초기 설정
void setup()
{
  Serial3.begin(115200); // ESP8266과의 통신 속도 설정 (115200bps)
  Serial3.setTimeout(5000); // ESP8266 응답 대기 시간 설정 (5초)
  Serial.begin(115200); // 시리얼 모니터와의 통신 속도 설정 (115200bps)

  SPI.begin(); // SPI 통신 초기화 (RFID)
  mfrc522.PCD_Init(); // RFID 리더기 초기화

  pinMode (servoPin, OUTPUT); // 서보모터 핀 출력 모드 설정
  myservo.attach(6); // 서보모터 핀 연결

  lcd.init(); // LCD 초기화
  lcd.backlight(); // LCD backlight ON

  connectWifi(); // Wi-fi 연결
  connectTCP(); // TCP 연결
}

// 메인 루프
void loop()
{ 
  lcd.setCursor(0,0); // lcd 출력 위치
  lcd.print("wait...  "); // lcd 초기 출력

 // RFID 카드 탐지
 if ( ! mfrc522.PICC_IsNewCardPresent()) {
  return; // 탐지하지 못하면 루프 종료
 }

 // RFID 카드 읽기
 if ( ! mfrc522.PICC_ReadCardSerial()) {
  return; // 읽지 못하면 루프 종료
 }

 mfrc522.PICC_DumpToSerial(&(mfrc522.uid)); //RFID 카드 정보 표시 (디버그)

  // UID 값을 문자열로 변환
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid += String(mfrc522.uid.uidByte[i], HEX); // UID의 각 바이트를 16진수로 변환 후 문자열에 추가
    if (i < mfrc522.uid.size - 1) uid += ":"; // ex) mfrc522.uid.uidByte = {0xAB, 0xCD, 0xEF, 0x12} 순서대로 더할 때 마지막만 : 제거
  }
  uid += String("2"); // client 구분용 데이터 추가
  Serial.println("RFID Scanned: " + uid); // UID 출력 (디버그)

  // 서버로 데이터 전송
  String data = "2"; // 인식 여부만 구분용 데이터 전송
  httpclient(data); // 데이터 전송 함수

  // ESP8266에서 서버 응답 확인
  while (Serial3.available()) {
    char ch = static_cast<char>(Serial3.read()); // 응답 수신
    Serial.print(ch); // 서버 응답 출력
    if(ch == 'Z'){ // 서버로부터 'Z' 응답이 오는 경우
      lcd.setCursor(0,0); // lcd 출력 위치
      lcd.print("DETECTED!"); // LCD 출력
      // 서보모터 제어
      for (pos = 0; pos <= 180; pos += 1) // 위에 변수를 선언한 pos는 0, 180도보다 작다면 , 1도씩 더함
      {
        myservo.write(pos); // 서보모터를 pos 각도로 작동
        delay(10); // 0.01초 대기 ( 1초 = 1000 )
      }
    
      for (pos = 180; pos >= 0; pos -= 1) // pos가 180이면, 0도보다 크다면 , 1도씩 빼기
      {
        myservo.write(pos); // 서보모터를 pos 각도로 작동
        delay(10); // 0.01초 대기 ( 1초 = 1000 )
      }
    }
  }
  Serial.println("");
  
  
  delay(2000); // 다음 loop 2초 대기
}
