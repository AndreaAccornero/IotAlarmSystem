#include <WiFi.h>
#include <HTTPClient.h>
#include "../credentials.h"

#define PRESSURE_SENSOR_PIN 33 // Pin collegato al sensore di pressione
#define LED_BUILTIN 2          // Pin del LED integrato
#define SPEAKER_PIN 32         // Pin collegato allo speaker
#define PRESSURE_THRESHOLD 4095 // Soglia per attivare LED e speaker

// Configurazione HTTP
const char* serverName = "http://192.168.1.108:5000/sensor_data"; // URL del server Flask

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
}

void loop() {
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
