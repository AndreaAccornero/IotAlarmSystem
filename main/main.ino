#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "../credentials.h"

// Definizioni dei pin
#define LED_PIN 2
#define SPEAKER_PIN 32

// Configurazione HTTP
// const char* serverName = "http://192.168.1.124:5000/sensor_data"; // URL del server Flask

// Configurazione MQTT
const char* mqtt_server = "192.168.1.124";
const int mqtt_port = 1883;
const char* mqtt_topic_new_alarm = "iot/bed_alarm/new_alarm";
const char* mqtt_topic_stop_alarm = "iot/bed_alarm/stop_alarm";
const char* mqtt_topic_location = "iot/bed_alarm/location_alarm";  

// Variabili globali
bool stopAlarmFlag = false;
bool alarmActive = false;
String selectedSound = "sound1";  // Suono predefinito
String currentLocation = "Bologna";  // Location predefinita

// Struttura per memorizzare una sveglia
struct Alarm {
  int hour;
  int minute;
  String frequency;
  bool active;
};

#define MAX_ALARMS 5
Alarm alarms[MAX_ALARMS];
int alarmCount = 0;

WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600); // Fuso orario UTC+1 (Italia)

// Funzione per connettersi alla rete WiFi
void setup_wifi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
}

// Funzione per riconnettersi al broker MQTT
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32AlarmClient")) {
      Serial.println(" connected");
      client.subscribe(mqtt_topic_new_alarm);
      client.subscribe(mqtt_topic_stop_alarm);
      client.subscribe(mqtt_topic_location); 
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

// Callback MQTT per ricevere nuove sveglie, stop_alarm e location
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("JSON parsing error: ");
    Serial.println(error.c_str());
    return;
  }

  if (strcmp(topic, mqtt_topic_new_alarm) == 0) {
    if (alarmCount < MAX_ALARMS) {
      alarms[alarmCount].hour = doc["alarm_time"].as<String>().substring(0, 2).toInt();
      alarms[alarmCount].minute = doc["alarm_time"].as<String>().substring(3, 5).toInt();
      alarms[alarmCount].frequency = doc["alarm_frequency"].as<String>();
      alarms[alarmCount].active = true;
      alarmCount++;
      Serial.println("New alarm added.");
    } else {
      Serial.println("Max alarm limit reached!");
    }
  }

  if (strcmp(topic, mqtt_topic_stop_alarm) == 0) {
    if (doc.containsKey("stop_alarm")) {
      stopAlarmFlag = doc["stop_alarm"];
      Serial.print("Stop Alarm Received: ");
      Serial.println(stopAlarmFlag);
    }
  }

  if (strcmp(topic, mqtt_topic_location) == 0) {
    if (doc.containsKey("location")) {
      currentLocation = doc["location"].as<String>();
      Serial.print("Updated location: ");
      Serial.println(currentLocation);
    }
  }
}

// **Funzione che ottiene il meteo da OpenWeatherMap usando il nome della città**
String getWeatherCondition(String city) {
  HTTPClient http;
  String api_url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&appid=" + weather_api_key + "&units=metric";

  http.begin(api_url);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.print("Weather API Response: ");
    Serial.println(payload);

    StaticJsonDocument<600> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      String weather_condition = doc["weather"][0]["main"].as<String>();
      http.end();
      return weather_condition;
    }
  }

  http.end();
  return "Clear";  // Valore di default se la richiesta fallisce
}

// **Sceglie il suono della sveglia in base al meteo**
void setAlarmSoundBasedOnWeather() {
  Serial.print("Using location: ");
  Serial.println(currentLocation);

  String weather_condition = getWeatherCondition(currentLocation);
  Serial.print("Weather Condition: ");
  Serial.println(weather_condition);

  if (weather_condition == "Clear") selectedSound = "sound1";  // Soleggiato = Energico
  else if (weather_condition == "Clouds") selectedSound = "sound2";  // Nuvoloso = Medio
  else if (weather_condition == "Rain") selectedSound = "sound3";  // Pioggia = Rilassante
  else if (weather_condition == "Snow") selectedSound = "sound4";  // Neve = Dolce
  else if (weather_condition == "Thunderstorm") selectedSound = "sound5";  // Temporale = Allerta
  else selectedSound = "sound1";  // Default

  Serial.print("Selected Alarm Sound: ");
  Serial.println(selectedSound);
}

// Controlla se una sveglia deve attivarsi
void checkAlarms() {
  timeClient.update();
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  bool anyAlarmActive = false;

  for (int i = 0; i < alarmCount; i++) {
    if (alarms[i].active && alarms[i].hour == currentHour && alarms[i].minute == currentMinute) {
      anyAlarmActive = true;
      Serial.println("Alarm triggered!");
      setAlarmSoundBasedOnWeather();
    }
  }

  if (anyAlarmActive) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("STA SUONANDO LA SVEGLIA!");
    tone(SPEAKER_PIN, 2000);

    if (stopAlarmFlag) {
      digitalWrite(LED_PIN, LOW);
      noTone(SPEAKER_PIN);
      Serial.println("LA SVEGLIA SI È SPENTA!");

      for (int i = 0; i < alarmCount; i++) {
        if (alarms[i].active) alarms[i].active = false;
      }
      stopAlarmFlag = false;
    }
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  reconnectMQTT();
  timeClient.begin();
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  checkAlarms();
  delay(1000);
}
