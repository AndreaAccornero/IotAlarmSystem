#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "../credentials.h"  // Contiene le credenziali WiFi

// Definizioni dei pin
#define LED_PIN 2

// Configurazione MQTT
const char* mqtt_server = "192.168.1.124";
const int mqtt_port = 1883;
const char* mqtt_topic_new_alarm = "iot/bed_alarm/new_alarm";
const char* mqtt_topic_stop_alarm = "iot/bed_alarm/stop_alarm";

// Flag globali
bool pressureSensorEnabled = false;  // Per eventuali estensioni con sensore di pressione
bool stopAlarmFlag = false;          // Flag per determinare se la sveglia deve essere fermata

// Struttura per memorizzare una sveglia
struct Alarm {
  int hour;
  int minute;
  String frequency;
  bool active;
  bool playing;
};

// Massimo numero di sveglie gestite
#define MAX_ALARMS 5
Alarm alarms[MAX_ALARMS];
int alarmCount = 0;

WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600); // Fuso orario UTC+1 (Italia)

// Funzione per connettersi alla rete WiFi
void setup_wifi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Funzione per riconnettersi al broker MQTT
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println(" connected");
      client.subscribe(mqtt_topic_new_alarm);
      client.subscribe(mqtt_topic_stop_alarm);
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

// Callback MQTT per ricevere nuove sveglie e il comando stop_alarm
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

  // Aggiunge una nuova sveglia
  if (strcmp(topic, mqtt_topic_new_alarm) == 0) {
    if (alarmCount < MAX_ALARMS) {
      alarms[alarmCount].hour = doc["alarm_time"].as<String>().substring(0,2).toInt();
      alarms[alarmCount].minute = doc["alarm_time"].as<String>().substring(3,5).toInt();
      alarms[alarmCount].frequency = doc["alarm_frequency"].as<String>();
      alarms[alarmCount].active = true;
      alarmCount++;
      Serial.println("New alarm added.");
    } else {
      Serial.println("Max alarm limit reached!");
    }
  } 
  
  // Ricezione del segnale di stop dell'allarme
  if (strcmp(topic, mqtt_topic_stop_alarm) == 0) {
    if (doc.containsKey("stop_alarm")) {
      stopAlarmFlag = doc["stop_alarm"];
      Serial.print("Stop Alarm Received: ");
      Serial.println(stopAlarmFlag);
    }
  }
}

// Funzione per verificare se la sveglia deve attivarsi in base alla frequenza
bool shouldAlarmActivate(int hour, int minute, String frequency) {
  time_t now = timeClient.getEpochTime();
  struct tm *timeInfo = localtime(&now);
  int currentDay = timeInfo->tm_wday; // 0=Sunday, 6=Saturday

  if (frequency == "everyday") return true;
  if (frequency == "weekdays" && currentDay >= 1 && currentDay <= 5) return true;
  if (frequency == "weekends" && (currentDay == 0 || currentDay == 6)) return true;
  if (frequency == "monday" && currentDay == 1) return true;
  if (frequency == "tuesday" && currentDay == 2) return true;
  if (frequency == "wednesday" && currentDay == 3) return true;
  if (frequency == "thursday" && currentDay == 4) return true;
  if (frequency == "friday" && currentDay == 5) return true;
  if (frequency == "saturday" && currentDay == 6) return true;
  if (frequency == "sunday" && currentDay == 0) return true;

  return false;
}

// Controlla e gestisce le sveglie attive
void checkAlarms() {
  timeClient.update();
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  bool anyAlarmActive = false;

  for (int i = 0; i < alarmCount; i++) {
    if (alarms[i].active && alarms[i].hour == currentHour && alarms[i].minute == currentMinute) {
      if (shouldAlarmActivate(alarms[i].hour, alarms[i].minute, alarms[i].frequency)) {
        anyAlarmActive = true;
        alarms[i].playing = true;
        Serial.println("Alarm triggered!");
      }
    }
  }

  // Se c'è almeno una sveglia attiva, accendi il LED
  if (anyAlarmActive) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("STA SUONANDO LA SVEGLIAAAAAAA");

    // Se il flag di stop è attivo, spegni la sveglia
    if (stopAlarmFlag) {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LA SVEGLIA SI E' SPENTAAAAAAA");

      // Disattiva tutte le sveglie che stavano suonando
      for (int i = 0; i < alarmCount; i++) {
        if (alarms[i].active && alarms[i].playing && alarms[i].hour == currentHour && alarms[i].minute == currentMinute) {
          alarms[i].active = false;
          alarms[i].playing = false;
        }
      }

      stopAlarmFlag = false; // Reset del flag
    }
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
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
  delay(1000); // Controlla le sveglie ogni secondo
}
