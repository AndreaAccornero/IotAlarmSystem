#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "../credentials.h"

#define LED_BUILTIN 2           // Pin del LED integrato
#define SPEAKER_PIN 32          // Pin collegato allo speaker
#define PRESSURE_SENSOR_PIN 33  // Pin collegato al sensore di pressione
#define PRESSURE_THRESHOLD 4060 // Soglia per attivare LED e speaker

// Configurazione porta MQTT
const int mqtt_port = 1883;   

// Configurazione topic MQTT
const char* mqtt_topic_sampling_rate = "iot/bed_alarm/sampling_rate"; 
const char* mqtt_topic_trigger_alarm = "iot/bed_alarm/trigger_alarm"; 
const char* mqtt_topic_stop_alarm = "iot/bed_alarm/stop_alarm"; 
const char* mqtt_topic_alarm_sound = "iot/bed_alarm/alarm_sound";

WiFiClient espClient;          
PubSubClient client(espClient); 

// Variabili con valori di default
bool alert_active = false;
unsigned long last_sample_time = 0;  
unsigned long sampling_rate = 5000;
unsigned int alarm_sound = 1; 

// Variabili per il calcolo della media dei valori del sensore
long pressureSum = 0;              
unsigned int pressureCount = 0;    

// Variabili per il calcolo della media della latenza one-way
unsigned long sumOneWayLatency = 0;
int countInstances = 0;

void setup() {
  Serial.begin(115200);

  // Configurazione dei pin
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(PRESSURE_SENSOR_PIN, INPUT);

  // Connessione WiFi
  connectToWiFi();

  // Configurazione MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback); 
  connectToMQTT();
}

void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  unsigned long current_time = millis(); 

  // Legge il valore dal sensore e accumula per la media
  int sensorValue = analogRead(PRESSURE_SENSOR_PIN);
  pressureSum += sensorValue;
  pressureCount++;

  // Se è trascorso l'intervallo di sampling_rate, calcola la media
  if (current_time - last_sample_time >= sampling_rate) {
    float pressureAverage = pressureSum / (float)pressureCount;
    Serial.print("Media dei valori: ");
    Serial.println(pressureAverage);

    // Gestione dell'allarme
    handleAlert(pressureAverage);

    // Invio dei dati al server e misurazione della latenza
    sendPressureData(pressureAverage);

    // Reset dei contatori per il prossimo intervallo
    pressureSum = 0;
    pressureCount = 0;
    last_sample_time = current_time;
  }
}

// Funzione per connettersi al WiFi
void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
}

// Funzione per connettersi al server MQTT
void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32Client")) { 
      Serial.println("connected");

      // Sottoscrizione ai topic MQTT
      client.subscribe(mqtt_topic_sampling_rate);
      client.subscribe(mqtt_topic_trigger_alarm);
      client.subscribe(mqtt_topic_stop_alarm);
      client.subscribe(mqtt_topic_alarm_sound);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds");
      delay(5000);
    }
  }
}

// Callback per la ricezione dei messaggi MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);

  char json[length + 1];
  strncpy(json, (char*)payload, length);
  json[length] = '\0'; 

  Serial.print("Message: ");
  Serial.println(json);

  StaticJsonDocument<200> doc; 
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("Error parsing JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Gestione dei messaggi ricevuti dai topic MQTT
  if (doc.containsKey("sampling_rate")) {
    int new_sampling_rate = doc["sampling_rate"];
    sampling_rate = new_sampling_rate * 1000;
    Serial.print("Updated sampling_rate: ");
    Serial.println(sampling_rate);
  }

  if (doc.containsKey("trigger_alarm")) {
    alert_active = true;
  }

  if (doc.containsKey("stop_alarm")) {
    if (alert_active) {
      alert_active = false;
    }
  } 

  if (doc.containsKey("alarm_sound")) {
    int new_alarm_sound = doc["alarm_sound"];
    alarm_sound = new_alarm_sound;
  }
}

// Funzione per inviare i dati al server Flask e misurare la latenza
void sendPressureData(int pressureValue) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");
    String payload = "{\"pressure_value\": " + String(pressureValue) + "}";
    
    // Registra il tempo prima dell'invio
    unsigned long tStart = millis();
    
    int httpResponseCode = http.POST(payload);
    
    // Registra il tempo subito dopo la risposta
    unsigned long tEnd = millis();
    
    http.end();
    
    // Calcola il Round-Trip Time (RTT) e la latenza one-way
    unsigned long rtt = tEnd - tStart;       
    float oneWayLatency = rtt / 2.0;           

    // Accumula la latenza per il calcolo della media
    sumOneWayLatency += oneWayLatency;
    countInstances++;

    // Calcola la media della latenza one-way per 3 istanze
    // if (countInstances >= 3) {
    //   float avgLatency = sumOneWayLatency / (float)countInstances;
    //   Serial.print("Media latenza one-way per 3 istanze: ");
    //   Serial.print(avgLatency);
    //   Serial.println(" ms");

    //   // Resetta i contatori per il prossimo calcolo
    //   sumOneWayLatency = 0;
    //   countInstances = 0;
    // }

    // Controlla la risposta del server
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("Dati inviati con successo. Codice di risposta: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Errore nell'invio dei dati. Codice di risposta: ");
      Serial.println(httpResponseCode);
    }
  } else {
    Serial.println("WiFi Disconnesso");
  }
}

// Funzione per gestire l'allarme  
void handleAlert(int pressureValue) {
  // L'allarme si attiva se il valore supera la soglia e l'allerta è attiva
  if (alert_active && pressureValue >= PRESSURE_THRESHOLD) {
    digitalWrite(LED_BUILTIN, HIGH); 
    playMelody(alarm_sound);     
  } else {
    digitalWrite(LED_BUILTIN, LOW);  
    noTone(SPEAKER_PIN);            
    alert_active = false; // Una volta suonata l'allerta, la disattiva
  }
}
  
// Funzione per suonare la melodia dell'allarme in base al tipo di suono
void playMelody(int soundType) {
  switch (soundType) {
    case 1: // Clear (Soleggiato) - Energico
      for (int i = 0; i < 5; i++) {
        tone(SPEAKER_PIN, 1000, 200);
        delay(250);
        tone(SPEAKER_PIN, 1200, 200);
        delay(250);
        tone(SPEAKER_PIN, 1400, 200);
        delay(250);
      }
      break;
    case 2: // Clouds (Nuvoloso) - Medio
      for (int i = 0; i < 10; i++) {
        tone(SPEAKER_PIN, 800, 300);
        delay(350);
        tone(SPEAKER_PIN, 900, 300);
      }
      break;
    case 3: // Rain/Drizzle (Pioggia/Piovigginoso) - Rilassante
      for (int i = 0; i < 10; i++) {
        tone(SPEAKER_PIN, 500, 400);
        delay(450);
        tone(SPEAKER_PIN, 600, 400);
        delay(450);
      }
      break;
    case 4: // Thunderstorm (Temporale) - Allerta
      for (int i = 0; i < 6; i++) {
        tone(SPEAKER_PIN, 2000, 100);
        delay(150);
        tone(SPEAKER_PIN, 2500, 100);
        delay(150);
        tone(SPEAKER_PIN, 3000, 100);
        delay(150);
        tone(SPEAKER_PIN, 1500, 300);
        delay(350);
      }
      break;
    default: // Default: Clear (Soleggiato) - Energico
      for (int i = 0; i < 5; i++) {
        tone(SPEAKER_PIN, 1000, 200);
        delay(250);
        tone(SPEAKER_PIN, 1200, 200);
        delay(250);
        tone(SPEAKER_PIN, 1400, 200);
        delay(250);
      }
      break;
  }
}
