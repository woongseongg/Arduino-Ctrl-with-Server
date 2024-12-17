// 초음파 센서, 부저 핀 설정
int echoPin = 9;
int trigPin = 10;
int buzzer = 13;
bool isConnected = false;

//  와이파이 접속 및 접속포트설정
String ssid = "jeongtong";
String PASSWORD = "12345678";
String host = "192.168.0.75";
String port = "9000";


// WiFi에 연결하는 함수
void connectWifi(){

  Serial3.println("AT");
  Serial.println(Serial3.read());

  // WiFi 명령어 구성 
  String join ="AT+CWJAP=\""+ssid+"\",\""+PASSWORD+"\"";
  Serial.println("Connect Wifi...");

  // 명령어 전송
  Serial3.println(join);
  delay(5000);

  // 성공 여부 확인
  if(Serial3.find("OK"))
  {
    Serial.print("WIFI connect\n");
    isConnected = true;
  }else
  {
    Serial.println("connect timeout\n");
    isConnected = false;
  }
  delay(1000);
}

// TCP 연결을 설정
void connectTCP(){
    delay(100);
    Serial.println("connect TCP...");

     // TCP 명령어 구성
    Serial3.println("AT+CIPSTART=\"TCP\",\""+host+"\","+port);
    delay(100);

     // 연결 오류 확인
    if(Serial.find("ERROR")) {
      isConnected = false;
      Serial.println("fail connect TCP...");
      return;
    }
    isConnected = true;
}


// HTTP 요청을 보내는 함수
void httpclient(String char_input){
// 연결이 끊어졌는지 확인하고 필요시 재연결
 if (isConnected==false) {
    Serial.println("Connection lost. Reconnecting...");
    connectWifi();
    connectTCP();
    if (!isConnected) return;
  }
  Serial.println("Send data...");
  String cmd=char_input;          // 전송할 데이터를 cmd 변수에 저장

   // 데이터 전송 준비
  Serial3.print("AT+CIPSEND=");
  Serial3.println(cmd.length());
  Serial.print("AT+CIPSEND=");
  Serial.println(cmd.length());

   // 전송 준비 완료 확인
  if(Serial3.find(">"))
  {
    Serial.print(">");
  }else
  {
    Serial.println("connect timeout");
    delay(100);
    isConnected = false;
    return;
    }

  delay(50);
  Serial3.println(cmd); // 데이터를 Serial3 포트를 통해 전송
  Serial.println(cmd);  // 데이터를 Serial 포트를 통해 전송
  
  delay(100);
}  
// 설정 함수
void setup()
{
  // 시리얼 통신 초기화 
  Serial3.begin(115200);
  Serial3.setTimeout(5000);
  Serial.begin(115200); 
  Serial.println("ESP8266 AT command test");

  // 핀모드 설정  
   pinMode(trigPin,OUTPUT);
  pinMode(echoPin,INPUT);
  pinMode(buzzer, OUTPUT);

  // WIFI , TCP 연결
  connectWifi();
  connectTCP();
  delay(500);
}


//Main 루프
void loop()
{ // 연결상태 확인 및 재연결
  if (isConnected==false) {
    Serial.println("Connection lost. Reconnecting...");
    connectWifi();
    connectTCP();
    delay(1000);
    return;
  }

  // 초음파센서 트리거
  digitalWrite(trigPin,HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin,LOW);
  float duration,distance;
  
// 초음파가 발사되어 물체에 반사되어 돌아오는 시간을 측정
  duration = pulseIn(echoPin, HIGH);

    // 거리 계산
   // 초음파의 속도 340, 밀리세컨드에서 세컨드로 10000
   // 초음파가 나갔다가 돌아오는 거리이므로 2로 나누어줌.
   distance = ((float)(340 * duration) / 10000) / 2;
   int readValue = distance;
  
  // // 전송데이터 준비
   String str_output = String(readValue*10 + 0) ;
   
  // 시리얼 모니터에 거리 출력
  Serial.print(readValue);
  Serial.println(" Cm(str_output printing before httpclient()");

  // 거리 데이터 전송
  httpclient(str_output);
  String message = "";
// 수신 데이터 읽기
while (Serial3.available()) {
    char ch = static_cast<char>(Serial3.read());
    Serial.print(ch);
    // 수신 데이터에 B문자가 포함되어있으면 부저가 울림
    if(ch == 'B'){
      digitalWrite(13, 255);
      }
    //수신 데이터에 K문자가 포함되어있으면 부저울림이 멈춤
      
    else if (ch == 'K'){
      digitalWrite(13, 0);
      }
  }
  }
