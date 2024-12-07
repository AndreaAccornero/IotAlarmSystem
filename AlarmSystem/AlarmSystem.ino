#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Definizione dei pin
const int speakerPin = 32;
const int fsrPin = 33;
const int ledPin = 2;

// Configurazione WiFi
#include <../credentials.h>
const char* mqtt_broker = "192.168.1.124";
const char* serverName = "http://192.168.1.124:5000/sensor_data";

WiFiClient espClient;
PubSubClient client(espClient);

// Variabili globali
int fsrValue = 0;
int threshold = 1;
long sampling_rate = 100000000000;
bool stop_alarm = true;
unsigned long last_sample_time = 0;

void mqtt_callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  String msg;


  for (int i = 0; i < length; i++) {
    msg += (char)message[i];
  }
  Serial.println("Message: " + msg);

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, msg);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  if (String(topic) == "iot/bed_alarm/stop_alarm" && doc.containsKey("stop_alarm")) {
    stop_alarm = doc["stop_alarm"].as<bool>();
    Serial.print("Stop alarm updated: ");
    Serial.println(stop_alarm);
  }

  if (String(topic) == "iot/bed_alarm/update_sampling_rate" && doc.containsKey("sampling_rate")) {
    long new_sampling_rate = doc["sampling_rate"].as<long>() * 1000;
    if (new_sampling_rate > 0) {
      sampling_rate = new_sampling_rate;
      Serial.print("Updated sampling rate to: ");
      Serial.println(sampling_rate);
    } else {
      Serial.println("Invalid sampling rate received");
    }
  }
}

void setup() {
  pinMode(speakerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  client.setServer(mqtt_broker, 1883);
  client.setCallback(mqtt_callback);
  connectToMQTT();
}

void connectToMQTT() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Connected to MQTT broker");
      client.subscribe("iot/bed_alarm/update_sampling_rate");
      client.subscribe("iot/bed_alarm/stop_alarm");
    } else {
      Serial.print("Failed MQTT connection, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 2 seconds");
      delay(2000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  // Lettura continua del sensore per attivare/disattivare il suono
  readFSRAndControlAlarm();

  unsigned long current_time = millis();

  // Invio dati al server Flask basato sul sampling rate
  if (current_time - last_sample_time >= sampling_rate) {
    last_sample_time = current_time;
    sendFSRDataToServer();
  }
}

void readFSRAndControlAlarm() {
  fsrValue = analogRead(fsrPin);
  Serial.print("FSR Value: ");
  Serial.println(fsrValue);
  if(stop_alarm == true){
      digitalWrite(ledPin, LOW);
      noTone(speakerPin);
  } else {
    if (fsrValue > 0) {
      digitalWrite(ledPin, LOW);
      noTone(speakerPin);
    } else {
      digitalWrite(ledPin, HIGH);
      tone(speakerPin, 300);
    }
  }
  delay(200); // Delay di 1 secondo
}

void sendFSRDataToServer() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"pressure_value\": " + String(fsrValue) + "}";
    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }

  Serial.print("Current sampling rate: ");
  Serial.println(sampling_rate);
  Serial.print("Current stop_alarm: ");
  Serial.println(stop_alarm);
}
