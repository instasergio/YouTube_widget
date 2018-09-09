/* Счётчик подписчиков канала YouTube
  by AlexGyver
  2017
  Реализовано: подключение к wifi с возможностью восстановить подключение, если оно было разорвано (без перезагрузки)
  Получение отпечатка SSL сертификата (запрос идёт каждый раз, когда не получается подключиться к google.api)
  Из ответа google.api получаем число подписчиков с любым количеством знаков (идёт отсечка по символу ")
  Запаздывающий фильтр обеспечивает плавную смену цвета с периодом 15 минут
*/
//-----------НАСТРОЙКИ-------------
const char* ssid = "LOGIN";           // имя wifi роутера / точки доступа
const char* password = "PASS";     // пароль wifi роутера / точки доступа
String channel_ID = "ID";  // ID канала (копировать из строки адреса канала)
String API_key = "API";  // API ключ аккаунта канала (как получить: https://www.slickremix.com/docs/get-api-key-for-youtube/ )
int max_gain = 180;   // число подписок в час, при котором цвет станет красным
//-----------НАСТРОЙКИ-------------

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

String host;
String httpsFingerprint = "";
String payload, sub_string;
int delta, color, brightness;
int mid_s, max_s;
int R, G, B;
int start_point, end_point;
float delta_f = 0.0;
float K = 0.97, k;
byte tries;
int i = 0, j = 0, httpCode;
unsigned long subscribers, new_subscribers = 0;
boolean display_mode = true, wifi_connect = false;
boolean start_flag = true;
unsigned long last_check, last_color, last_Submins, last_BL, last_mode, wifi_try;
WiFiClientSecure client;

unsigned long delta_hour[60], delta_day[24], total_hour_subs, total_day_subs;
unsigned long minute_subs;
unsigned long last_minute;
byte minutes = 0, hours = 0;
unsigned long min_step = 3;    // считать прирост за указанное число минут

#include <LiquidCrystal_I2C.h>
#include <Wire.h>
LiquidCrystal_I2C lcd(0x3F,16,2);

//---- настрйока пинов-----
#define ledR 14
#define ledG 13
#define ledB 12
#define BL_pin 16
//---- настрйока пинов-----

void setup () {
  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
  host = "www.googleapis.com";
  Serial.begin(115200);
  pinMode(A0, INPUT);
  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledB, OUTPUT);
  pinMode(BL_pin, OUTPUT);
  Wire.begin(5, 4);
  analogWrite(BL_pin, 1023);

  lcd.begin();
  lcd.display();
  lcd.backlight();
}

void get_color() {
  color = delta_f * 10;
  if (color <= mid_s) {
    k = map(color, 0, mid_s, 0, 1023);
    B = 1023 - k;
    G = k;
    R = 0;
  }
  if (color > mid_s) {
    k = map(color, mid_s, max_s, 0, 1023);
    if (k > 1023) k = 1023;
    B = 0;
    G = 1023 - k;
    R = k;
  }
}

unsigned long getSubs() {
  unsigned long title = 0;
  String headers = "";
  String body = "";
  bool finishedHeaders = false;
  bool currentLineIsBlank = true;
  bool gotResponse = false;
  long now;
  
  if (client.connect(host, 443)) {
    Serial.println("connected");
    
    String URL = "/youtube/v3/channels?part=statistics&key=" + API_key + channel_ID;

    Serial.println(URL);
    
    client.println("GET " + URL + " HTTP/1.1");
    client.print("Host: "); client.println(host);
    client.println("User-Agent: arduino/1.0");
    client.println("");

    now = millis();
    // checking the timeout
    while (millis() - now < 1500) {
      while (client.available()) {
        char c = client.read();
        Serial.print(c);

        if (finishedHeaders) {
          body=body+c;
        } else {
          if (currentLineIsBlank && c == '\n') {
            finishedHeaders = true;
          }
          else {
            headers = headers + c;
          }
        }

        if (c == '\n') {
          currentLineIsBlank = true;
        }else if (c != '\r') {
          currentLineIsBlank = false;
        }

        //marking we got a response
        gotResponse = true;
        
      }
      if (gotResponse) {

        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(body);
        if (root.success()) {
          if (root.containsKey("items")) {
            title = root["items"][0]["statistics"]["subscriberCount"].as<unsigned long>();
          } 
        } else {
          Serial.println("failed to parse JSON");
        }

        break;
      }
    }
  }
    
  return title;
}

void loop() {
  ESP.wdtFeed();
  if (!wifi_connect || WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi...");
    WiFi.begin(ssid, password);
    wifi_try = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if (millis() - wifi_try > 10000) break;
    }
    if (WiFi.status() != WL_CONNECTED) {      
      lcd.clear();
      lcd.print("Connect failed");
      delay(500);
      wifi_connect = false;
    } else {
      lcd.clear();
      lcd.print("WiFi connected");
      delay(500);
      wifi_connect = true;
    }
  }
  
 if (start_flag && wifi_connect) {
   max_gain = max_gain / (60 / min_step);  // узнать пороговое число подписок за min_step
   min_step = (long)min_step * 60000;
   mid_s = max_gain * 5;
   max_s = max_gain * 10;
   new_subscribers = getSubs();
   subscribers = new_subscribers;
   minute_subs = new_subscribers;
   start_flag = false;
 }
 if (millis() - last_mode > 10000) {
   display_mode = !display_mode;
   last_mode = millis();
 }
 if (millis() - last_check > 6000 && wifi_connect) {
   getSubs();
   switch (display_mode) {
     case 0:
       lcd.clear();
       lcd.setCursor(2, 0);
       lcd.print("Subscribers:");
       lcd.setCursor(5, 1);
       lcd.print(new_subscribers);
       break;
     case 1:
       lcd.clear();
       lcd.setCursor(0, 0);
       lcd.print("Last hour:");
       lcd.setCursor(11, 0);
       lcd.print(total_hour_subs);
       lcd.setCursor(0, 1);
       lcd.print("Last day:");
       lcd.setCursor(11, 1);
       lcd.print(total_day_subs);
       break;
   }
   last_check = millis();
 }
 if (millis() - last_Submins > min_step && wifi_connect) {
   delta = (long)new_subscribers - subscribers;
   if (delta < 0) delta = 0;
   subscribers = new_subscribers;
   last_Submins = millis();
 }
 if (millis() - last_color > 3000 && wifi_connect) {
   delta_f = delta_f * K + delta * (1 - K);
   get_color();
   // отладка
   /*Serial.print("delta: "); Serial.print(delta); Serial.print(", ");
     Serial.print("delta_f: "); Serial.print(delta_f); Serial.print(", ");
     Serial.print("R: "); Serial.print(R); Serial.print(", ");
     Serial.print("G: "); Serial.print(G); Serial.print(", ");
     Serial.print("B: "); Serial.println(B);*/
   brightness = map(analogRead(A0), 140, 1024, 1023, 200);
   if (brightness > 1023) brightness = 1023;
   if (brightness < 200) brightness = 200;
   analogWrite(BL_pin, brightness);
   analogWrite(ledR, R * brightness / 1023);
   analogWrite(ledG, G * brightness / 1023);
   analogWrite(ledB, B * brightness / 1023);
   last_color = millis();
 }
 if (millis() - last_minute > 60000 && wifi_connect) {
   delta_hour[minutes] = (long)new_subscribers - minute_subs;
   if (delta_hour[minutes] < 0) delta_hour[minutes] = 0;
   minute_subs = new_subscribers;
   total_hour_subs = 0;
   for (int i = 0; i < 59; i++) {
     total_hour_subs += delta_hour[i];
   }
   delta_day[hours] = total_hour_subs;
   total_day_subs = 0;
   for (int i = 0; i < 23; i++) {
     total_day_subs += delta_day[i];
   }
   minutes++;
   if (minutes > 59) {
     minutes = 0;
     hours++;
     if (hours > 23) hours = 0;
   }
   last_minute = millis();
 }
}
