#include <custom.h> // Includes my credentials outside git-repo, delete this line in your project!
#include "sml.h"
#include <RingBuf.h> // RingBuffer von https://github.com/Locoduino/RingBuffer

#include <stdio.h>
#include <string.h>
// Wifi+OTA
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

// Time:
#include <WiFiUdp.h>
#include <NTPClient_Generic.h> // https://github.com/khoih-prog/NTPClient_Generic
#include <Timezone_Generic.h>  // https://github.com/khoih-prog/Timezone_Generic
#include <TimeLib.h>           // https://github.com/PaulStoffregen/Time

#include <HTTPClient.h>

// MQTT Client
#include <PubSubClient.h>
#include <ArduinoMqttClient.h>
#include "WiFiClientSecure.h"
#include "HardwareSerial.h"

#define LED_WIFI 23 // Wlan-Status LED
#define LED_DATA 22 // Datenstatus LED
#define LED_PIN_A  27 // 27 Pin-Eingabe LED, BiColor Rot+Weis, A > B = Weis, A < B = Rot
#define LED_PIN_B  5  // Pin-Eingabe LED, BiColor Rot+Weis, A > B = Weis, A < B = Rot

#define Taster 25
#define Serial_Meter_RX 16
#define Serial_Meter_TX 17 // 17
#define numOfValues      3       // 1.8.0, 2.8.0 und 16.7.0 
#define MAX_BUF_SIZE 700



#ifndef SSID_NAME
#define SSID_NAME "<insert wifi name here>"
#define SSID_PASS "<insert wifi password here>"

#define METER_PIN 0, 0, 0, 0 // Order like in the mail or the display

#define TIBBER_ID "xxxxxxxxxxxxx"
#define BRIDGE_ID "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

const char* CA_cert = \
                      "-----BEGIN CERTIFICATE-----\n" \
                      "################################################################\n" \
                      "################################################################\n" \
                      "################################################################\n" \
                      "-----END CERTIFICATE-----";

const char* ESP_CA_cert = \
                          "-----BEGIN CERTIFICATE-----\n" \
                          "################################################################\n" \
                          "################################################################\n" \
                          "################################################################\n" \
                          "-----END CERTIFICATE-----";

const char* ESP_RSA_key = \
                          "-----BEGIN RSA PRIVATE KEY-----\n" \
                          "################################################################\n" \
                          "################################################################\n" \
                          "################################################################\n" \
                          "-----END RSA PRIVATE KEY-----";

#endif

sml_states_t currentState;
IPAddress dns(10, 0, 83, 254);
IPAddress dns2(8, 8, 8, 8);

//IPAddress mqtt_server(52, 51, 178, 179);
const char* mqtt_server = "a1zhmn1192zl1a.iot.eu-west-1.amazonaws.com"; // "a1zhmn1192zl1a.iot.eu-west-1.amazonaws.com";
//IPAddress mqtt_server_ip;
int mqtt_port = 8883;

RingBuf<unsigned char, MAX_BUF_SIZE> MeterBuf;

const char* ssid = SSID_NAME;
const char* password = SSID_PASS;





int httpResponseCode = 0;

int meter_pin_set[] = {METER_PIN};
int skipmenu_set = 13;
int skipmenu;
int meter_pin[4];
int pinseq = 0;

//unsigned char b[MAX_BUF_SIZE];
//int char_pos = 0;
//int BufferPos = 0;
unsigned int secs = 0;
unsigned int oldsecs = 0;
unsigned int dezisecs = 0;
unsigned int olddezisecs = 0;
int wifistatus = 0;

unsigned int total_buf = 0;
unsigned int progress_buf = 0;
unsigned int progress_old = 0;



int led_mode_data = 2; // 0 = Aus, 1 = An, 2 = Blinken im Sekundentakt
int led_mode_wifi = 2; // 0 = Aus, 1 = An, 2 = Blinken im Sekundentakt
int led_mode_pin = -1; // 0 = Aus, 1 = Weis, 2 = Rot, 3 = Blinken Weis, 4 = Blinken Rot, 5 = Wechselblinken

/*typedef struct {
  char name[8];
  char hextxt[18];
  int  offset;
  int  length;
  int  divisor;
  } SMLKEY;*/


#define MAX_STR_MANUF 5
unsigned char manuf[MAX_STR_MANUF];
double Zaehler_Bezug = 0, Zaehler_Einspeisung = 0;
double Zaehler_Wert = 0;

typedef struct {
  const unsigned char OBIS[6];
  void (*Handler)();
} OBISHandler;
char buffer[50];
char floatBuffer[20];

void Manufacturer() {
  smlOBISManufacturer(manuf, MAX_STR_MANUF);
}
void Power_Bezug() {
  smlOBISWh(Zaehler_Bezug);
}
void Power_Einspeisung() {
  smlOBISWh(Zaehler_Einspeisung);
}
void Power_Wert() {
  smlOBISW(Zaehler_Wert);
}

OBISHandler OBISHandlers[] = {
  {{0x81, 0x81, 0xc7, 0x82, 0x03, 0xff}, &Manufacturer},           /*   1-  1: 96. 50.1*255 */
  {{0x01, 0x00, 0x01, 0x08, 0x00, 0xff}, &Power_Bezug},            /*   1-  0:  1.  8.0*255 */
  {{0x01, 0x00, 0x02, 0x08, 0x00, 0xff}, &Power_Einspeisung},      /*   1-  0:  2.  8.0*255 */
  {{0x01, 0x00, 0x10, 0x07, 0x00, 0xff}, &Power_Wert},             /*   1-  0: 16.  7.0*255 */
  {{0}, 0}
};


int EOM = 1;
int ota_run = 0;
int test_pin = 0;
hw_timer_t * timer = NULL;
hw_timer_t * timer_pin = NULL;

const int debugport = 8880;
WiFiServer server_debug(debugport);
WiFiClient client_debug;


HTTPClient http;
WiFiClient client_plain;
WiFiClientSecure client_secure;
//PubSubClient mqtt_client(mqtt_server, mqtt_port, mqtt_callback, client_secure);
MqttClient client_mqtt(client_secure);


/*void mqtt_callback(char* topic, byte* payload, unsigned int length) {

  client_debug.println("Received MQTT!");
  client_debug.print("Topic: ");
  client_debug.println(topic);
  client_debug.print("Length: ");
  client_debug.println(length);
  client_debug.print("Payload: ");
  for (int i = 0; i < sizeof(payload); i++) {
    client_debug.print(String(payload[i]));
  }
  client_debug.println();
  // handle message arrived
  }*/


const char* serverName = "http://10.0.83.1/input.zaehler.php";


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
Timezone *CE;
time_t t;
int meter_last = 0;
int skip = 0;
HardwareSerial   Meter(2);
void IRAM_ATTR  pinsequenz() { // Kurz = < 4,5 Sekunden (< 45 Sequenzschritte); Lang > 4,5 Sekunden (> 45 Sequenzschritte)
  if (pinseq > 0) {
    switch (pinseq) {
      case 1: // Sequenzzstart, Lampe an fürs Menü / Lampentest; Sekunde 0
        white_led(1);
        meter_pin[0] = meter_pin_set[0];
        meter_pin[1] = meter_pin_set[1];
        meter_pin[2] = meter_pin_set[2];
        meter_pin[3] = meter_pin_set[3];
        skipmenu = skipmenu_set;
        break;
      case 6: // 100 ms später Lampe wieder aus
        white_led(0);
        break;

      case 10: // 100 ms später Lampe wieder aus
        white_led(1);
        break;
      case 11: // 100 ms später Lampe wieder aus
        white_led(0);
        break;
      case 20: // 2 Sekunden später gehts los, 4. Pin einblinken.
        //showpin();
        if (meter_pin[0] > 0) { // Wert noch nicht 0, also noch einmal Leuchten
          white_led(1);
        }
        break;
      case 25:
        if (meter_pin[0] > 0) { // 500ms Später Licht aus
          meter_pin[0] -= 1;
          white_led(0);
          pinseq = 15; // Rücksprung und prüfen ob noch eine erhöhung folgt.
        }
        break;

      case 55: // 5 Sekunden später gehts weiter, 3. Pin einblinken.
        showpin();
        if (meter_pin[1] > 0) { // Wert noch nicht 0, also noch einmal Leuchten
          white_led(1);
        }
        break;
      case 60:
        if (meter_pin[1] > 0) { // 500ms Später Licht aus
          meter_pin[1] -= 1;
          white_led(0);
          pinseq = 50; // Rücksprung und prüfen ob noch eine erhöhung folgt.
        }
        break;
      case 90: // 5 Sekunden später gehts los, 2. Pin einblinken.
        showpin();
        if (meter_pin[2] > 0) { // Wert noch nicht 0, also noch einmal Leuchten
          white_led(1);
        }
        break;
      case 95:
        if (meter_pin[2] > 0) { // 500ms Später Licht aus
          meter_pin[2] -= 1;
          white_led(0);
          pinseq = 85; // Rücksprung und prüfen ob noch eine erhöhung folgt.
        }
        break;
      case 125: // 5 Sekunden später gehts weiter, 1. Pin einblinken.
        showpin();
        if (meter_pin[3] > 0) { // Wert noch nicht 0, also noch einmal Leuchten
          white_led(1);
        }
        break;
      case 130:
        if (meter_pin[3] > 0) { // 500ms Später Licht aus
          meter_pin[3] -= 1;
          white_led(0);
          pinseq = 120; // Rücksprung und prüfen ob noch eine erhöhung folgt.
        }
        break;
      case 160: // 5 Sekunden später gehts ins menü, nun wird etwas gesprungen.
        if (skipmenu > 0) {
          white_led(1);
        }
        break;
      case 165:
        if (skipmenu > 0) { // 500ms Später Licht aus
          skipmenu -= 1;
          white_led(0);
          pinseq = 155; // Rücksprung und prüfen ob noch eine erhöhung folgt.
        }
        break;
      case 200: // 5 Sekunden später gehts weiter und inf wird auf off setzt..
        white_led(1);
        break;
      case 250:
        white_led(0);
        break;
      case 255: // noch 2x Kurz blinken für Rücksprung ins Hauptmenü.
        white_led(1);
        break;
      case 260:
        white_led(0);
        break;
      case 265: // noch 1x Kurz blinken für Rücksprung ins Hauptmenü.
        white_led(1);
        break;
      case 270:
        white_led(0);
        digitalWrite(LED_PIN_A, 0);
        digitalWrite(LED_PIN_B, 0);
        digitalWrite(Serial_Meter_TX, 1);
        pinseq = -1;
        break;

      case 350: // Nur zur Sicherheit.
        white_led(0);
        pinseq = -1;
        break;
    }
    pinseq += 1;
  }
}
void IRAM_ATTR onTimer() {
  if (ota_run == 0) {
    // Increment the counter and set the time of ISR

    if (led_mode_data > 1) {
      digitalWrite(LED_DATA, !digitalRead(LED_DATA));
    }
    else {
      digitalWrite(LED_DATA, !led_mode_data); // Low Aktiv
    }
    if (led_mode_wifi > 1) {
      digitalWrite(LED_WIFI, !digitalRead(LED_WIFI));
    }
    else {
      digitalWrite(LED_WIFI, !led_mode_wifi); // Low Aktiv
    }
    /*  switch (led_mode_pin) {
        case 0:
          digitalWrite(LED_PIN_A, 0);
          digitalWrite(LED_PIN_B, 0);
          break;
        case 1:
          digitalWrite(LED_PIN_A, 1);
          digitalWrite(LED_PIN_B, 0);
          break;
        case 2:
          digitalWrite(LED_PIN_A, 0);
          digitalWrite(LED_PIN_B, 1);
          break;
        case 3:
          digitalWrite(LED_PIN_A, !digitalRead(LED_PIN_A));
          digitalWrite(LED_PIN_B, 0);
          break;
        case 4:
          digitalWrite(LED_PIN_A, 0);
          digitalWrite(LED_PIN_B, !digitalRead(LED_PIN_B));
          break;
        case 5:
          digitalWrite(LED_PIN_A, digitalRead(LED_PIN_B));
          digitalWrite(LED_PIN_B, !digitalRead(LED_PIN_A));
          break;*/
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  pinMode(LED_WIFI, OUTPUT);
  pinMode(LED_DATA, OUTPUT);
  pinMode(Taster, INPUT);
  pinMode(LED_PIN_A, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  pinMode(Serial_Meter_TX, OUTPUT);
  digitalWrite(Serial_Meter_TX, 1);
  timer = timerBegin(0, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, onTimer, true);
  timerAlarmWrite(timer, 1000000, true); // 1 sec
  timerAlarmEnable(timer);

  timer_pin = timerBegin(1, 80, true);
  timerAttachInterrupt(timer_pin, pinsequenz, true);
  timerAlarmWrite(timer_pin, 100000, true); // 100ms
  timerAlarmEnable(timer_pin);

  Serial.println("Wifi:");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setHostname("ESP32_ELMO");
  ArduinoOTA.setPassword(OTA_PASS);

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    ota_run = 1;
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    if (client_debug.connected())
    {
      //client_debug.println("Start updating " + type);
      client_debug.printf("%s: Start updating: %s", timeClient.getFormattedTime(), type);
      client_debug.println();
    }
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    if (client_debug.connected())
    {
      client_debug.printf("\n%s: End", timeClient.getFormattedTime());
      client_debug.println();
      client_debug.stop();
    }
    Serial.println("\nEnd");
    ota_run = 0;
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    if (total_buf == 0) total_buf = total / 100;
    progress_buf = progress / total_buf;
    if (progress_buf != progress_old) {
      check_debug();
      progress_old = progress_buf;
      if (client_debug.connected())
      {
        client_debug.printf("Progress: %u%%\r", progress_buf);
      }
      Serial.printf("Progress: %u%%\r", progress_buf);
    }
  })
  .onError([](ota_error_t error) {
    if (client_debug.connected())
    {
      client_debug.printf("%s: Error[%u]: ", timeClient.getFormattedTime(), error);
      if (error == OTA_AUTH_ERROR) client_debug.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) client_debug.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) client_debug.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) client_debug.println("Receive Failed");
      else if (error == OTA_END_ERROR) client_debug.println("End Failed");
    }
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");

  });

  ArduinoOTA.begin();

  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns, dns2);
  CE    = new Timezone(CEST, CET);
  timeClient.begin();
  server_debug.begin();
  timeClient.setTimeOffset(3600);
  while (millis() < 10000) {
    ArduinoOTA.handle();
    check_debug();
  }

  Meter.begin(9600, SERIAL_8N1, Serial_Meter_RX, -1);
  pinMode(Taster, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(Taster), Taster_Click, FALLING);

  client_secure.setInsecure();
  client_secure.setCACert(CA_cert);
  client_secure.setCertificate(ESP_CA_cert);
  client_secure.setPrivateKey(ESP_RSA_key);
  // mqtt_client.setServer(mqtt_server, mqtt_port);

  client_mqtt.setId(TIBBER_ID);
}

void loop() {
  check_debug();
  client_mqtt.poll();
  // mqtt_client.loop();

  dezisecs = millis() / 100;
  if (dezisecs != olddezisecs) { // 100ms
    olddezisecs = dezisecs;
    //pinsequenz();
  }
  secs = dezisecs / 10;
  if (secs != oldsecs) { // 1 sec
    oldsecs = secs;
    /*client_debug.printf("Manuf: %s", manuf);
      client_debug.println();
      dtostrf(Zaehler_Bezug / 1000, 0, 1, floatBuffer);
      client_debug.printf("Bezug: %skWh", floatBuffer);
      //client_debug.printf("Bezug: %.1fkWh", Zaehler_Bezug);
      client_debug.println();
      dtostrf(Zaehler_Einspeisung / 1000, 0, 1, floatBuffer);
      client_debug.printf("Einspeisung: %skWh", floatBuffer);
      //client_debug.printf("Einspeisung: %.1fkWh", Zaehler_Einspeisung);
      client_debug.println();
      dtostrf(Zaehler_Wert, 10, 1, floatBuffer);
      client_debug.printf("Aktuell: %sW", floatBuffer);
      client_debug.println();*/
    if ((timeClient.getUTCEpochMillis() / 1000) - meter_last > 15) {
      client_debug.print(timeClient.getFormattedTime() + ": ");
      client_debug.println("Keine Sensordaten");
      led_mode_data = 0;
      pinseq = 0;
      digitalWrite(Serial_Meter_TX, 1);
      digitalWrite(LED_PIN_A, 0);
      digitalWrite(LED_PIN_B, 0);
    }
  }
  wifistatus = WiFi.status();
  if (wifistatus == WL_CONNECTED) {
    led_mode_wifi = 1;
    ArduinoOTA.handle();
    timeClient.update();
  }
  else {
    led_mode_wifi = 2;
  }
  while (Meter.available() > 0) {
    // unsigned char c = Meter.read();
    //  b[char_pos] = c;
    //  char_pos += 1;
    //  b[char_pos] = '\0';
    readByte(Meter.read());
  }
}

void showPwr() {
  client_debug.printf("%s: Manuf: %s", timeClient.getFormattedTime(), manuf);
  client_debug.println();
  dtostrf(Zaehler_Bezug / 1000, 0, 1, floatBuffer);
  client_debug.printf("%s: Bezug: %skWh", timeClient.getFormattedTime(), floatBuffer);
  //client_debug.printf("Bezug: %.1fkWh", Zaehler_Bezug);
  client_debug.println();
  dtostrf(Zaehler_Einspeisung / 1000, 0, 1, floatBuffer);
  client_debug.printf("%s: Einspeisung: %skWh", timeClient.getFormattedTime(), floatBuffer);
  //client_debug.printf("Einspeisung: %.1fkWh", Zaehler_Einspeisung);
  client_debug.println();
  dtostrf(Zaehler_Wert, 10, 1, floatBuffer);
  client_debug.printf("%s: Aktuell: %sW", timeClient.getFormattedTime(), floatBuffer);
  client_debug.println();
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  WiFi.begin(ssid, password);
}
void Taster_Click() {
  pinseq = 1;
}
void check_debug() {
  if (server_debug.hasClient()) {
    if (client_debug.connected()) {
      server_debug.accept().println("Busy");
    }
    else {
      client_debug = server_debug.accept();
      client_debug.printf("%s: Debug Port", timeClient.getFormattedTime());
      client_debug.println("");
      //  DNSClient dns;
      client_debug.printf("%s: DNS: ", timeClient.getFormattedTime());
      client_debug.println(WiFi.dnsIP());
      /*      client_debug.println("CA Cert: ");
            client_debug.println(CA_cert);
            client_debug.println("Client Cert: ");
            client_debug.println(ESP_CA_cert);
            client_debug.println("Client Key: ");
            client_debug.println(ESP_RSA_key);*/
    }
  }
}
void readByte(unsigned char currentChar)
{
  unsigned int iHandler = 0;
  currentState = smlState(currentChar);
  if (currentState == SML_START) {
    /* b[0] = 0x1B;
      b[1] = 0x1B;
      b[2] = 0x1B;
      char_pos = 3; */
    led_mode_data = 2; // Blinken
    MeterBuf.clear();
    MeterBuf.push(0x1B);
    MeterBuf.push(0x1B);
    MeterBuf.push(0x1B);
  }
  else {
    if (MeterBuf.size() < MAX_BUF_SIZE) {
      MeterBuf.push(currentChar);
    }
    else {
      client_debug.print(F(">>> Message larger than MAX_BUF_SIZE\n"));
      Meter.flush();
      MeterBuf.clear();
      led_mode_data = 2; // Blinken
    }
  }
  if (currentState == SML_LISTEND) {
    /* check handlers on last received list */
    for (iHandler = 0; OBISHandlers[iHandler].Handler != 0 &&
         !(smlOBISCheck(OBISHandlers[iHandler].OBIS));
         iHandler++)
      ;
    if (OBISHandlers[iHandler].Handler != 0) {
      OBISHandlers[iHandler].Handler();
    }
  }
  else if (currentState == SML_UNEXPECTED) {
    //  client_debug.print(F(">>> Unexpected byte\n"));
    led_mode_data = 2; // Blinken
  }
  else if (currentState == SML_FINAL) {
    // client_debug.print(F(">>> Successfully received a complete message!\n"));
    led_mode_data = 1; // Leuchten lassen
    // print_buffer();
    if (WiFi.isConnected()) {
      showPwr();

      // if (skip == 0) {

      //   for (int pos = 0; pos < char_pos; pos++) {
      //     client_debug.write(b[pos]);
      //mqtt_client.publish(TIBBER_PUB, BufToCStr().c_str());
      //   }


      //  skip = 2;
      if (!client_mqtt.connected()) {
        client_debug.printf("%s: Trying to connect...", timeClient.getFormattedTime());
        if (client_mqtt.connect(TIBBER_HOST, TIBBER_PORT)) {
          client_debug.println("Success");
        }
        else {
          client_debug.println("Error");
          client_debug.printf("%s: %s", timeClient.getFormattedTime(), client_mqtt.connectError());
          client_debug.println();
        }
      }
      if (client_mqtt.connected()) {
        client_debug.printf("%s: Start Message", timeClient.getFormattedTime());
        client_debug.println();
        client_mqtt.beginMessage(TIBBER_PUB);
        int BufSize = MeterBuf.size();
        for (int pos = 0; pos < BufSize; pos++) {
          client_mqtt.write(MeterBuf[pos]);
        }
        client_debug.printf("%s: End Message", timeClient.getFormattedTime());
        client_debug.println(client_mqtt.endMessage());
        // client_debug.println("Send.");
      }

      /*
            if (!mqtt_client.connected()) {
              if (mqtt_client.connect(TIBBER_ID)) {
                //  mqtt_client.subscribe("tibber-bridge/ef7c17ecb5fa4dedb31718cfe0aab39d/receive");
              }
              else {
                client_debug.print("Connect failed!  mqtt_client state:");
                client_debug.println(mqtt_client.state());
                client_debug.print("WiFiClientSecure client state:");
                char lastError[100];
                client_secure.lastError(lastError, 100); //Get the last error for WiFiClientSecure
                client_debug.println(lastError);
              }
            }
            if (mqtt_client.connected()) {
              client_debug.println("Sammle Daten...");
              client_debug.println("Buffer Size:");
              int BufSize = MeterBuf.size();
              client_debug.println(BufSize);
              mqtt_client.beginPublish(TIBBER_PUB, BufSize, true);
              for (int pos = 0; pos < BufSize; pos++) {
                mqtt_client.write(MeterBuf[pos]);
                //mqtt_client.publish(TIBBER_PUB, BufToCStr().c_str());
              }
              client_debug.println("Sende Daten");
              mqtt_client.endPublish();
              client_debug.println("Erledigt!");
              client_debug.print("Verbindung:");
              client_debug.println(mqtt_client.connected());
            }*/
      // }
      // else {
      //   skip = skip - 1;
      // }
      meter_last = timeClient.getUTCEpochMillis() / 1000;
      String httpRequestData = "id=";
      httpRequestData += "ESP32_ELMO";
      httpRequestData += "&ntptime=";
      httpRequestData += String(meter_last);
      httpRequestData += "&raw=";


      httpRequestData += BufToString();
      http.setReuse(true);
      http.begin(client_plain, serverName);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      httpResponseCode = http.POST(httpRequestData);
      if (httpResponseCode != 200) {
        client_debug.printf("%s: HTTP ERROR: %i", timeClient.getFormattedTime(), httpResponseCode);
        client_debug.println();
      }
      //  client_debug.print("HTTP Status ");
      //  client_debug.println(httpResponseCode);
    }
  }
  else if (currentState == SML_CHECKSUM_ERROR) {
    //  client_debug.print(F(">>> Checksum error.\n"));
    led_mode_data = 2; // Blinken
  }
}



void print_buffer()
{
  unsigned int i = 0;
  unsigned int j = 0;
  char b[5];
  client_debug.print(F("Size: "));
  client_debug.print(MeterBuf.size());
  client_debug.println("");
  client_debug.println(F("--- "));
  for (j = 0; j < MeterBuf.size(); j++) {
    i++;
    sprintf(b, "0x%02X", MeterBuf[j]);
    client_debug.print(b);
    if (j < MeterBuf.size() - 1) {
      client_debug.print(", ");
    }
    else {
      client_debug.println("");
      client_debug.println(F("--- "));
    }
    if ((i % 15) == 0) {
      client_debug.println("");
      i = 0;
    }
  }
}
/*String BufToCStr() {
  unsigned int j = 0;
  String out = "";
  char b[1];
  //client_debug.println(MeterBuf.size());
  for (j = 0; j < MeterBuf.size(); j++) {
    sprintf(b, "%c", MeterBuf[j]);
    out += String(b);
  }
  //client_debug.println(out);
  return out;
  }*/

String BufToString () {
  unsigned int j = 0;
  String out = "";
  char b[3];
  //client_debug.println(MeterBuf.size());
  for (j = 0; j < MeterBuf.size(); j++) {
    sprintf(b, "%02X", MeterBuf[j]);
    //out += String(b);
    out += b;
  }
  //client_debug.println(out);
  return out;
}
void showpin() {
  client_debug.print("Seq: ");
  client_debug.println(pinseq);
  client_debug.print("Pin: ");
  client_debug.print(meter_pin[3]);
  client_debug.print(meter_pin[2]);
  client_debug.print(meter_pin[1]);
  client_debug.println(meter_pin[0]);
}

void white_led (int status) {
  digitalWrite(Serial_Meter_TX, !status);
  digitalWrite(LED_PIN_A, status);
  digitalWrite(LED_PIN_B, !status);
}
