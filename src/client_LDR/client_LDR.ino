int LED = 13; // LED 핀 번호
#define BTN_PIN 2 // 버튼 핀 번호

int status = 0; // 상태 플래그 (0: 작동, 1: 중지)

// Wi-fi 설정
String ssid = "jeongtong"; // Wi-fi 이름
String PASSWORD = "12345678"; // Wi-fi 비밀번호
String host = "192.168.0.75"; // 서버 주소
String port = "9000"; // 서버 포트 번호

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
 
  pinMode(LED, OUTPUT); // LED 핀 출력 모드 설정
  
  pinMode(BTN_PIN, INPUT); // 버튼 핀 입력 모드 설정

  connectWifi();
  connectTCP();
}

// 메인 루프
void loop()
{
  int readValue = analogRead(A0); // A0 핀에서 아날로그 값 읽기 (조명 센서)

  // 전송할 데이터 생성
  String str_output = String(readValue*10+1); // A0 값에 계산 적용, 전송 데이터 생성

  // 서버로 데이터 전송
  httpclient(str_output);

  // ESP8266에서 서버 응답 확인
  while (Serial3.available()) {
    char ch = static_cast<char>(Serial3.read()); // 응답 수신
    Serial.print(ch); // 서버 응답 출력
    if(ch == 'L') // 서버로부터 'L' 응답이 오는 경우 (LED_ON)
      digitalWrite(13, 255); // LED 켜기
    else if(ch == 'K') // 서버로부터 'K' 응답이 오는 경우 (OK)
      digitalWrite(13, 0); // LED 끄기
  }
  Serial.println("");

  // 버튼을 눌렀을 때
  if (digitalRead(BTN_PIN)){
    Serial.println("stop");
    Serial.println("Press button to restart");
    delay(1000);
    status = 1; // 상태를 중지로 변경
  }


  while (status){
    delay(500); // 0.5초 딜레이
    if (digitalRead(BTN_PIN)){ // 버튼이 눌리면
      Serial.println("return");
      status = 0; // 상태를 작동으로 변경
      delay(1200); // 1.2초 대기
      break;
    }
    delay(10);
  }
  delay(100); // 다음 loop 0.1초 대기
}
