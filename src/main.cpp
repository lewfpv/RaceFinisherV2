#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include "message.h"
#include "addpeers.h"

String ProgVer = "2.2";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define RGB_LED_PIN 14
#define LED_BUILTIN 22

#define NUM_LEDS 1
#define VBAT_PIN 33
#define BUTTON_PIN 12
#define VIBRA_PIN 13

volatile bool IsPressed = false;
volatile bool IsFinished = false;
volatile int RoundCounter = 0;  // gombnyomasok száma

//RACER INFO STRUCT
struct RacerInfo {
  uint8_t id;
  String racerName;
  String channel;
};
RacerInfo currentRacer;

//Battery INFO
float battV = 3.3;
const float alpha = 0.03;
volatile float last_batt_avg = 3.3;

//OLED Display
Adafruit_NeoPixel ws2812b(NUM_LEDS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// --- FUNKCIÓK ---
RacerInfo getRacerInfo() {
  String mac = WiFi.macAddress();

  if(mac == "08:3A:8D:96:40:58") {
    return {1, "Racer 1", "R1"};
  }
  if(mac == "B0:B2:1C:F8:B5:C4") {
    return {2, "Racer 2", "R3"};
  }
  if(mac == "B0:B2:1C:F8:BC:48") {
    return {3, "Racer 3", "R6"};
  }
  if(mac == "B0:B2:1C:F8:AE:88") {
    return {4, "Racer 4", "R7"};
  }

  // Alapértelmezett, 'ismeretlen' érték
  return {0, "Unknown", "Unknown"};
}

// --- BUTTON INTERRUPT ---
void IRAM_ATTR OnButtonPress() {
  static unsigned long last_interrupt_time = 0;
  unsigned long t = millis();
  if(t - last_interrupt_time > 300){
    IsPressed = true;
  }
  last_interrupt_time = t;
}

void GetVoltage() {
  float current_volt = ((float)analogRead(VBAT_PIN) / 4095) * 3.3 * 2 * 1.06;
  battV = (current_volt * alpha) + (last_batt_avg * (1-alpha));
  last_batt_avg = battV;
}

String GetBatteryVoltage() {
  GetVoltage();
  return "Batt: " + String(battV, 1) + "V";
}

void DisplayDefault(){
    display.clearDisplay();
    display.invertDisplay(false);
    display.setTextSize(1);
    display.setCursor(1,2);
    display.println("Tiny Drones");
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(28,25);
    display.println(currentRacer.racerName);
    display.setTextSize(1);
    display.setCursor(1,52);
    GetVoltage();
    display.print("V");
    display.print(ProgVer);
    display.setCursor(102,52);
    display.print(String(battV,1));
    display.println("V");
    //DrawCounter();
    display.display();
}

void DisplayStart() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2); //felo sor
    display.setCursor(1, 2);
    display.println(currentRacer.channel);
    int16_t x, y;
    uint16_t w, h;
    display.setTextSize(3);
    display.getTextBounds("Racer ?", 0,0, &x, &y, &w, &h);
    display.setCursor((SCREEN_WIDTH - w)/2, (SCREEN_HEIGHT - h)/2);
    display.println(currentRacer.racerName);
    display.setTextSize(1); //also sor
    display.setCursor(1, 52);
    display.println(GetBatteryVoltage());
    display.setTextSize(1); //also sor jobb oldal
    display.setCursor(83, 52);
    display.println("fw: " + String(ProgVer));
    display.display();
}

void DisplayLargeNumber(int number) {
    display.clearDisplay();
    display.setTextSize(6);
    display.setTextColor(SSD1306_WHITE);
    int16_t x, y;
    uint16_t w, h;
    String numStr = String(number);
    display.getTextBounds(numStr, 0,0, &x, &y, &w, &h);
    display.setCursor((SCREEN_WIDTH - w)/2, (SCREEN_HEIGHT - h)/2);
    display.println(numStr);
    display.display();
}

void DisplayFinished() {
 Serial.println("Sent with success");
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(4,25);
    display.setTextColor(SSD1306_WHITE);
    display.println("*FINISHED*"); //display.println("* OK! *");
    display.setTextSize(1);
    display.setCursor(1,53);
    display.println(GetBatteryVoltage());
    display.display();
}

// --- ESP-NOW ---
void SendNOW(const uint8_t *mac, const Message &msg) {
  esp_err_t result = esp_now_send(mac, (uint8_t *)&msg, sizeof(msg));
  Serial.printf("Sent message: type=%d index=%d value=%d\n", msg.type, msg.index, msg.value);
  (result == ESP_OK) ? Serial.println("✅ OK") : Serial.println("❌ ERROR");
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  if(status == ESP_NOW_SEND_SUCCESS){
    Serial.println("Delivery Success");
  } else {
    Serial.println("Delivery Fail");
  }
}

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
    MessageLong msg;
    memcpy(&msg, incomingData, sizeof(msg));
    switch (msg.type) {
      case 9: { // Verseny üzenet
          IsPressed = false;
          IsFinished = false;
          RoundCounter = 0;
        if (msg.index > 0){    //reset és név ujrairas
         currentRacer.racerName = msg.p;
        } 
        default:
        break;
      }
    }
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  Serial.println("Setup...");
  Wire.begin(26, 27);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VBAT_PIN, INPUT);
  pinMode(VIBRA_PIN, OUTPUT);
  digitalWrite(VIBRA_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  Serial.println("Tiny Drones - Race Finisher");
  Serial.println("firmware version: " + String(ProgVer));
  Serial.println("MAC: " + WiFi.macAddress());

  currentRacer = getRacerInfo();
  Serial.println(currentRacer.racerName);

  if(display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED OK");
    DisplayDefault();
  } else {
    Serial.println("OLED FAIL!");
  }


  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    display.clearDisplay();
    
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("ESP-NOW init error!");
    display.display();
    return;
  }
  else{
    esp_now_register_send_cb(OnDataSent);
    addPeers();
    esp_now_register_recv_cb(OnDataRecv);
    Serial.println("OnDataRecv -> Registred..");
  }
  
  // Gomb beallitas
  Serial.print("Init button -->  ");
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), OnButtonPress, FALLING);
  Serial.println("attachInterrupt -> Added");

  ws2812b.begin();
  ws2812b.setBrightness(130);
  ws2812b.clear();
  ws2812b.setPixelColor(0, ws2812b.Color(255, 255, 255));
  ws2812b.show();
  delay(2000);
  ws2812b.setBrightness(0); //led kikapcs!
  ws2812b.clear();
  ws2812b.setPixelColor(0, ws2812b.Color(255, 0, 0));
  ws2812b.show();
  digitalWrite(VIBRA_PIN, LOW);
  Serial.println("Setup FINISHED!");
}

// --- LOOP ---
void loop() {
    if(IsPressed) {
        IsPressed = false;
        RoundCounter++;

        if(RoundCounter == 1) { //1.KÖR TELJESÍTVE
            DisplayLargeNumber(1);
            ws2812b.clear();
            ws2812b.setPixelColor(0, ws2812b.Color(0, 255, 0)); // zöld LED
            ws2812b.show();
            Message msg = {1, currentRacer.id, 1}; // type,index,value
            SendNOW(receiver1, msg);
            digitalWrite(VIBRA_PIN, HIGH);
            delay(250);
            digitalWrite(VIBRA_PIN, LOW);
        }
        else if(RoundCounter == 2) { //2.KÖR TELJESÍTVE
            DisplayLargeNumber(2);
            ws2812b.clear();
            ws2812b.setPixelColor(0, ws2812b.Color(0, 255, 0)); // zöld LED
            ws2812b.show();
            Message msg = {1, currentRacer.id, 2};
            SendNOW(receiver1, msg);
            digitalWrite(VIBRA_PIN, HIGH);
            delay(250);
            digitalWrite(VIBRA_PIN, LOW);
        }
        else if(RoundCounter == 3) { //FINISH
            DisplayFinished();
            ws2812b.setBrightness(130);
            ws2812b.clear();
            ws2812b.setPixelColor(0, ws2812b.Color(0, 255, 0)); // zöld LED
            ws2812b.show();
            Message msg = {1, currentRacer.id, 3};
            SendNOW(receiver1, msg);
            IsFinished = true;
            digitalWrite(VIBRA_PIN, HIGH);
            delay(800);
            digitalWrite(VIBRA_PIN, LOW);
        }
        else if(RoundCounter > 3) {
          IsPressed = false;
          IsFinished = false;
          RoundCounter = 0;
    }
  }
    // kezdőképernyő, ha még nem indult a verseny
    if(RoundCounter == 0) {
        DisplayStart();
        ws2812b.setBrightness(0);
        ws2812b.clear();
        ws2812b.setPixelColor(0, ws2812b.Color(0, 255, 0)); // zöld LED
        ws2812b.show();
    }

    delay(50);
}