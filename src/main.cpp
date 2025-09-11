#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include "message.h"
#include "addpeers.h"

String ProgVer = "2.3";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define RGB_LED_PIN 14
#ifndef LED_BUILTIN
  #define LED_BUILTIN 22
#endif
#define NUM_LEDS 1
#define VBAT_PIN 33
#define BUTTON_PIN 12
#define VIBRA_PIN 13

volatile bool IsPressed = false;
volatile bool IsFinished = false;
volatile int RoundCounter = 0;  // gombnyomások száma

String unitID = "NoDef";
String unitCh = "R?";
float battV = 3.3;
const float alpha = 0.03;
volatile float last_batt_avg = 3.3;

uint8_t serverAddress[] = {0x30, 0x30, 0xF9, 0x59, 0xE7, 0xE8}; // server MAC
esp_now_peer_info_t peerInfo;

Adafruit_NeoPixel ws2812b(NUM_LEDS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

typedef struct {
    uint8_t modeNum;
    volatile bool isReset;
} incoming_struct;

incoming_struct serverMessage;

// --- FUNKCIÓK ---
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  if(status == ESP_NOW_SEND_SUCCESS){
    Serial.println("Delivery Success");
  } else {
    Serial.println("Delivery Fail");
  }
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

void DisplayStart() {
    display.clearDisplay();
    display.setTextSize(6);
    display.setTextColor(SSD1306_WHITE);
    int16_t x, y;
    uint16_t w, h;
    display.getTextBounds("R3", 0,0, &x, &y, &w, &h);
    display.setCursor((SCREEN_WIDTH - w)/2, (SCREEN_HEIGHT - h)/2);
    display.println("R3");
  display.setTextSize(1);
  display.setCursor(1, 52);
  display.println(GetBatteryVoltage());
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


String RacerSelector() {
  String mac = WiFi.macAddress();
  unitCh = " - R?";
  if(mac == "08:3A:8D:96:40:58") return "Racer 1";
  if(mac == "B0:B2:1C:F8:B5:C4") return "Racer 2";
  if(mac == "B0:B2:1C:F8:BC:48") return "Racer 3";
  if(mac == "B0:B2:1C:F8:AE:88") return "Racer 4";
  return "Unknown";
}

// --- ESP-NOW ---
void SendNOW(const uint8_t *mac, const Message &msg) {
  esp_err_t result = esp_now_send(mac, (uint8_t *)&msg, sizeof(msg));
  Serial.printf("Sent message: type=%d index=%d value=%d\n", msg.type, msg.index, msg.value);
  (result == ESP_OK) ? Serial.println("✅ OK") : Serial.println("❌ ERROR");
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

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  Wire.begin(26, 27);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VBAT_PIN, INPUT);
  pinMode(VIBRA_PIN, OUTPUT);
  digitalWrite(VIBRA_PIN, LOW);

  WiFi.mode(WIFI_STA);
  Serial.println("Tiny Drones - Race Finisher");
  Serial.println("MAC: " + WiFi.macAddress());

  if(display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED OK");
    display.clearDisplay();
    display.display();
  }

  unitID = RacerSelector();

  if(esp_now_init() != ESP_OK){
    Serial.println("ESP-NOW init ERROR");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  addPeers();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), OnButtonPress, FALLING);

  ws2812b.begin();
  ws2812b.setBrightness(130);
  ws2812b.clear();
  ws2812b.setPixelColor(0, ws2812b.Color(0, 255, 0));
  ws2812b.show();
}

// --- LOOP ---
void loop() {
    if(IsPressed) {
        IsPressed = false;
        RoundCounter++;

        if(RoundCounter == 1) {
            

            // Rövid vibráció jelzés
    digitalWrite(VIBRA_PIN, HIGH);
    delay(100);
    digitalWrite(VIBRA_PIN, LOW);
DisplayLargeNumber(1);
                  ws2812b.clear();
      ws2812b.setPixelColor(0, ws2812b.Color(0, 255, 0)); // zöld LED
      ws2812b.show();
            Message msg = {1, 2, 1}; // type,index,value
            SendNOW(receiver1, msg);
        }
        else if(RoundCounter == 2) {

          // Rövid vibráció jelzés
    digitalWrite(VIBRA_PIN, HIGH);
    delay(100);
    digitalWrite(VIBRA_PIN, LOW);

            DisplayLargeNumber(2);
                  ws2812b.clear();
      ws2812b.setPixelColor(0, ws2812b.Color(0, 255, 0)); // zöld LED
      ws2812b.show();
            Message msg = {1, 2, 2};
            SendNOW(receiver1, msg);
        }
        else if(RoundCounter == 3) {



            DisplayFinished();
                  ws2812b.clear();
      ws2812b.setPixelColor(0, ws2812b.Color(0, 255, 0)); // zöld LED
      ws2812b.show();
            Message msg = {1, 2, 3};
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
        ws2812b.setPixelColor(0, ws2812b.Color(0, 255, 0)); // zöld LED
      ws2812b.show();
    }

    delay(50);

}