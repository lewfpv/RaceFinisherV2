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

// IDŐMÉRÉS
unsigned long startTime = 0;
unsigned long startTimeF = 0;
bool timingActive = false;
String laptime1;
String laptime2;
String laptime3;
String laptimetotal;
unsigned int finishCount;

String pilotlist[] = {"Pilotname1", "Pilotname2"};
const int numPilots = sizeof(pilotlist) / sizeof(pilotlist[0]);
int currentPilotIndex = 0;
unsigned long lastDisplayTime = 0;
const long interval = 1000; // 1 sec

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

int GetBatteryPercentage(){
  GetVoltage();

  // A minimum és maximum feszültség, ami a 0% és 100%-nak felel meg
  float minVoltage = 3.3;
  float maxVoltage = 4.1;

  // A feszültség leképezése 0-100 közötti százalékos értékre
  int percentage = map(battV * 100, minVoltage * 100, maxVoltage * 100, 0, 100);

  // Érték korlátozása 0 és 100 közé, hogy elkerüljük a negatív vagy 100-nál nagyobb értékeket
  if (percentage < 0) {
    percentage = 0;
  }
  if (percentage > 100) {
    percentage = 100;
  }

  return percentage;
}



void drawBattery(int x, int y, int percentage) {
  int width = 20;  // Ikon szélessége
  int height = 10; // Ikon magassága

  // Akkumulátor házának rajzolása
  display.drawRect(x, y, width, height, SSD1306_WHITE);

  // A plusz pólus rajzolása
  display.fillRect(x + width, y + 2, 2, 6, SSD1306_WHITE);

  // A töltöttségi szint kiszámítása és rajzolása
  int chargeWidth = map(percentage, 0, 100, 0, width - 4);
  
  if (percentage > 0) {
    display.fillRect(x + 2, y + 2, chargeWidth, height - 4, SSD1306_WHITE);
  }
}

String removeAllSpaces(char *str) {
  char *src = str, *dst = str;
  while (*src) {
    if (*src != ' ') { // ha nem szóköz, megtartjuk
      *dst++ = *src;
    }
    src++;
  }
  *dst = '\0'; // lezárjuk a végén
  return String(str);
}

void DisplayDefault(){
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(1,2);
    display.println("Tiny Drones");
    display.setTextSize(2);
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
    display.setCursor(1, 0);
    if (timingActive){
    display.println(currentRacer.channel +" ->");
    } else {
    display.println(currentRacer.channel);
    }
    int16_t x, y;
    uint16_t w, h;
    display.setTextSize(3);
    display.getTextBounds("Racer ?", 0,0, &x, &y, &w, &h);
    display.setCursor((SCREEN_WIDTH - w)/2, (SCREEN_HEIGHT - h)/2);
    display.println(currentRacer.racerName);
    //display.println(pilotName);
    display.setTextSize(1); //also sor
    display.setCursor(85, 4);
    display.println(String(GetBatteryPercentage()) + "%");
    //display.println(GetBatteryVoltage());
    drawBattery(104, 3, GetBatteryPercentage());
    display.setTextSize(1); //also sor jobb oldal
    display.setCursor(104, 56);
    display.println("v" + String(ProgVer)); //23x7 pixel
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

void DisplayPOST(String laptime1, String laptime2, String laptime3, String laptimetotal){

 // flag 32x16 icon
const unsigned char flag[] PROGMEM = {
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0xf0,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f,
  0x0f
};

// Example usage with Adafruit SSD1306 library:
// display.clearDisplay();
// display.drawBitmap(0, 0, flag, 32, 16, WHITE);
// display.display();


    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2); //felo sor
    display.setCursor(1, 0);
    display.println(currentRacer.racerName);
    display.drawLine(1, 18, 127, 18, SSD1306_WHITE);
    display.setTextSize(1); //also sor
    display.setCursor(65, 27);
    display.println(laptime1);
    display.setCursor(65, 36);
    display.println(laptime2);
    display.setCursor(65, 45);
    display.println(laptime3);
    display.setCursor(65, 56);
    display.println(laptimetotal);
    display.setCursor(10, 34);
    display.setTextSize(2);
    //display.println(currentRacer.channel);
    display.println(String(finishCount));
    display.drawBitmap(4, 52, flag, 32, 16, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(42, 52);
    display.println(currentRacer.channel);
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

          currentRacer.racerName = removeAllSpaces(msg.p);
          //Serial.printf("Pilóta név (szóköz nélkül): '%s'\n", removeAllSpaces(msg.p));
          
          Serial.println(currentRacer.racerName);
          IsPressed = false;
          IsFinished = false;
          RoundCounter = 0;
          laptime1 = "";
          laptime2 = "";
          laptime3 = "";
          laptimetotal = "";
          timingActive = false;
          startTime = millis();
          startTimeF = millis();

        if (msg.index > 0){    //reset és név ujrairas
         //currentRacer.racerName = msg.p;
        } 
      break;
      }
      case 1:{
      //időzités indítva:
      if (!timingActive) {
        startTime = millis();
        startTimeF = millis();
        timingActive = true;
        Serial.println("RACE STARTED - Időmérés elindítva!");
      }
      }
      case 6:{
      finishCount = msg.index;
      }
      default:
      break;
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
  digitalWrite(VIBRA_PIN, LOW); //vibra start

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

String getLaptime(int lap, unsigned long startTime){
  unsigned long elapsedTime = millis() - startTime;
  // Kiszámítjuk a percet, másodpercet és a századmásodpercet
      unsigned long minutes = elapsedTime / 60000;
    unsigned long seconds = (elapsedTime % 60000) / 1000;
    unsigned long hundredths = (elapsedTime % 1000) / 10;
    // Kiszámítjuk a percet, másodpercet és a századmásodpercet
        // Kiírjuk az eredményt a soros monitorra
    String lap1; 
        // A köridő szöveggé formázása és eltárolása
    if(lap == 4){lap1 = "+";} else {lap1 = lap;}
    lap1 += ": ";
    lap1 += String(minutes);
    lap1 += ":";
    if (seconds < 10) lap1 += "0";
    lap1 += String(seconds);
    lap1 += ",";
    if (hundredths < 10) lap1 += "0";
    lap1 += String(hundredths);
    return lap1;
    //Serial.printf("Eredmény: %lu:%02lu,%02lu\n", minutes, seconds, hundredths);
}

// --- LOOP ---
void loop() {
    if(IsPressed) {
        IsPressed = false;
        RoundCounter++;

        if(RoundCounter == 1) { //1.KÖR TELJESÍTVE
          if (timingActive) {
            laptime1 = getLaptime(1, startTime);
            Serial.println(laptime1);
            startTime = millis();
            }
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
            if (timingActive) {
            laptime2 = getLaptime(2, startTime);
            Serial.println(laptime2);
            startTime = millis();
            }
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
            if (timingActive) {
            laptime3 = getLaptime(3, startTime);
            laptimetotal = getLaptime(4, startTimeF);
            Serial.println(laptime3);
            Serial.println(laptimetotal);
            }
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
            delay(800);
            //POST SCREEN with timings
            DisplayPOST(laptime1, laptime2, laptime3, laptimetotal);
            timingActive = false;
            IsPressed = false;
            IsFinished = false;
        }
        
      
        
  }
    // kezdőképernyő, ha még nem indult a verseny
    if(RoundCounter == 0) {
        DisplayStart();

    //        if (millis() - lastDisplayTime >= interval) {
    //     Frissítjük a kijelzőt a következő pilóta nevével
    //    currentPilotIndex++;
    //    if (currentPilotIndex >= numPilots) {
    //        currentPilotIndex = 0; // Vissza az elejére
    //    }
    //    DisplayStart(pilotlist[currentPilotIndex]);
    //    lastDisplayTime = millis();
    //}


        ws2812b.setBrightness(0);
        ws2812b.clear();
        ws2812b.setPixelColor(0, ws2812b.Color(0, 255, 0)); // zöld LED
        ws2812b.show();
    }

    delay(50);
}