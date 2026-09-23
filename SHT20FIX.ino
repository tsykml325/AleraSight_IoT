#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HardwareSerial.h>
#include <DFRobot_RTU.h>

// ================== NODE ==================
#define NODE_ID "ESP32_NODE_01"

// ================== WIFI ==================
const char* ssid = "NAMA_WIFI";
const char* password = "PW_WIFI";

// ================== TELEGRAM ==================
#define BOT_TOKEN "ISI_TOKEN_BOT"
#define CHAT_ID "ISI_CHAT_ID"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// ================== RS485 ==================
#define RXD2 16
#define TXD2 17

HardwareSerial SerialRS485(2);
DFRobot_RTU Modbus_Master(&SerialRS485);

uint16_t regTemp;
uint16_t regHum;

// ================== VARIABLE ==================
unsigned long lastRead = 0;
const long interval = 3000;

bool dangerStatus = false;

float temperature = 0;
float humidity = 0;
// float suhuKalibrasi = 0;
// float humiKalibrasi = 0;

// =================================================
void setup() {

  Serial.begin(115200);

  // ================== RS485 ==================
  SerialRS485.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // ================== WIFI ==================
  WiFi.begin(ssid, password);

  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");

  }

  Serial.println("\nWiFi Connected");                         

  // ================== TELEGRAM ==================
  String startMsg = "SYSTEM STARTED\n";
  startMsg += "Node : " + String(NODE_ID);

  bot.sendMessage(CHAT_ID, startMsg, "");
}

// =================================================
void loop() {

  if (millis() - lastRead >= interval) {

    lastRead = millis();

    readPLC();

    // readMQ2();
  }
}

// =================================================
void readPLC() {

  // ================= TEMPERATURE =================
  regTemp = Modbus_Master.readInputRegister(1, 1);

  delay(20);

  // ================= HUMIDITY =================
  regHum = Modbus_Master.readInputRegister(1, 2);

  delay(20);

  // ================= CONVERT =================
  temperature = regTemp / 10.0;
  humidity    = regHum / 10.0;

  // Kalibrasi
  suhuKalibrasi = (suhuKalibrasi + (temperature - 3.5)) / 2;
  humiKalibrasi = (humiKalibrasi + (humidity + 9.8)) / 2;

  // Format hasil
  suhuKalibrasi = round(suhuKalibrasi * 10) / 10.0;  // 1 angka desimal
  humiKalibrasi = round(humiKalibrasi);                // bilangan bulat
  
  // ================= SERIAL MONITOR =================
  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Hum: ");
  Serial.print(humidity);
  Serial.println(" %");
}
