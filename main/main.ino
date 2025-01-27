#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "../credentials.h"

#define PRESSURE_SENSOR_PIN 33 // Pin collegato al sensore di pressione
#define LED_BUILTIN 2          // Pin del LED integrato
#define SPEAKER_PIN 32         // Pin collegato allo speaker
#define PRESSURE_THRESHOLD 4095 // Soglia per attivare LED e speaker

// Configurazione HTTP
const char* serverName = "http://192.168.1.124:5000/sensor_data"; // URL del server Flask

// Configurazione MQTT
const char* mqtt_server = "192.168.1.124"; // Indirizzo IP del broker MQTT
const int mqtt_port = 1883;               // Porta del broker MQTT
const char* mqtt_topic = "iot/bed_alarm/sampling_rate"; // Topic per ricevere il nuovo sampling_rate

WiFiClient espClient;          // Client WiFi
PubSubClient client(espClient); // Oggetto MQTT

// Variabili globali
unsigned long last_sample_time = 0;  // Memorizza il timestamp dell'ultima lettura
unsigned long sampling_rate = 5000; // Intervallo tra le letture (in millisecondi)
bool alert_active = false;          // Stato attuale dell'allarme (LED e speaker)

void setup() {
  // Inizializzazione del monitor seriale per il debug
  Serial.begin(115200);

  // Configura i pin
  pinMode(PRESSURE_SENSOR_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);

  // Connessione WiFi
  connectToWiFi();

  // Configurazione MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback); // Imposta la funzione di callback per i messaggi MQTT

  connectToMQTT();

}

void loop() {

   // Mantiene la connessione MQTT attiva
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  unsigned long current_time = millis(); // Ottiene il tempo corrente

  // Controlla se è trascorso il tempo impostato dal sampling_rate
  if (current_time - last_sample_time >= sampling_rate) {
    last_sample_time = current_time;

    // Legge il valore dal sensore di pressione
    int sensorValue = analogRead(PRESSURE_SENSOR_PIN);

    // Stampa il valore letto sul monitor seriale
    Serial.print("Valore del sensore: ");
    Serial.println(sensorValue);

    // Gestisce il LED e lo speaker in base al valore del sensore
    handleAlert(sensorValue);

    // Invia i dati al server Flask
    sendPressureData(sensorValue);
    
  }
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
}

void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32Client")) { // Nome univoco per il dispositivo MQTT
      Serial.println("connected");

      // Sottoscrive al topic per ricevere aggiornamenti sul sampling_rate
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);

  // Converte il payload in una stringa
  char json[length + 1];
  strncpy(json, (char*)payload, length);
  json[length] = '\0'; // Termina la stringa

  Serial.print("Message: ");
  Serial.println(json);

  // Parsing del JSON
  StaticJsonDocument<200> doc; // Crea un buffer JSON con una dimensione adeguata
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("Error parsing JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Estrae il valore di "sampling_rate"
  if (doc.containsKey("sampling_rate")) {
    int new_sampling_rate = doc["sampling_rate"];
    sampling_rate = new_sampling_rate * 1000; // Moltiplica per 1000
    Serial.print("Updated sampling_rate: ");
    Serial.println(sampling_rate);
  } else {
    Serial.println("Key 'sampling_rate' not found in JSON");
  }
}

void sendPressureData(int pressureValue) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Configura la richiesta HTTP
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    // Crea il payload JSON
    String payload = "{\"pressure_value\": " + String(pressureValue) + "}";

    // Invia la richiesta POST
    int httpResponseCode = http.POST(payload);

    // Controlla la risposta del server
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("Data sent successfully. Response code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error sending data. HTTP response code: ");
      Serial.println(httpResponseCode);
    }

    // Chiude la connessione HTTP
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void handleAlert(int pressureValue) {
  if (pressureValue >= PRESSURE_THRESHOLD) {
    // Attiva lo stato di allarme
    alert_active = true;
  } else {
    // Disattiva lo stato di allarme se il valore è sotto la soglia
    alert_active = false;
  }

  // Controlla lo stato dell'allarme per gestire LED e speaker
  if (alert_active) {
    digitalWrite(LED_BUILTIN, HIGH); // Accende il LED
    tone(SPEAKER_PIN, 2000);         // Suona lo speaker a 2000 Hz
  } else {
    digitalWrite(LED_BUILTIN, LOW);  // Spegne il LED
    noTone(SPEAKER_PIN);             // Ferma lo speaker
  }
}
