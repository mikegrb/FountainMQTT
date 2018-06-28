
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

const char* ssid        = "";
const char* password    = "";
const char* mqtt_server = "192.168.86.121";
const char* topic       = "kitchen/fountain/level";
const int   level_pin   = A3;
const int   led_pin     = 13;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

bool led_state = false;
long last_send = 0;

// smoothing via average
const int numReadings = 10;
int readings[numReadings];
int readIndex = 0;
int total = 0;

void setup() {
  Serial.begin(115200);
  analogSetPinAttenuation(level_pin, ADC_11db);
  pinMode(led_pin, OUTPUT);

  int initialRead = analogRead(level_pin);
  total = initialRead * numReadings;
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = initialRead;
  }
  
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  ArduinoOTA.setHostname("fountain");
  ArduinoOTA.setPasswordHash("e4af7c17be96d50f0c4766bffc337beb");
  ArduinoOTA.begin();

  mqtt_client.setServer(mqtt_server, 1883);
}

void reconnect_mqtt() {
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt_client.connect("ESP32Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqtt_client.state());
      delay(5000);
    }
  }
}

void loop() {
  if (!mqtt_client.connected()) {
    reconnect_mqtt();
  }
  ArduinoOTA.handle();
  mqtt_client.loop();

  long now = millis();
  long delta = now - last_send;
  if (abs(delta) > 30000) { // 30 sec between sends
    last_send = now;
    led_state = !led_state;
    digitalWrite(led_pin, led_state);
    
    total = total - readings[readIndex];
    readings[readIndex] = analogRead(level_pin);
    total = total + readings[readIndex];
    int average = total / numReadings;
    Serial.print(average);
    Serial.print(" ");

    readIndex = readIndex + 1;
    if (readIndex >= numReadings) {
      readIndex = 0;
    }

    int level = map(average, 0, 4095, 0, 1200); // convert to ~mm of liquid * 10
    char msg[5];
    snprintf(msg, 5, "%i", level);

    Serial.println(msg);
    mqtt_client.publish(topic, msg);
  }
}