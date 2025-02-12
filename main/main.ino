  #include <WiFi.h>
  #include <HTTPClient.h>
  #include <PubSubClient.h>
  #include <ArduinoJson.h>
  #include "../credentials.h"

  #define PRESSURE_SENSOR_PIN 33 // Pin collegato al sensore di pressione
  #define LED_BUILTIN 2          // Pin del LED integrato
  #define SPEAKER_PIN 32         // Pin collegato allo speaker
  #define PRESSURE_THRESHOLD 0 // Soglia per attivare LED e speaker

  // Configurazione HTTP
  const char* serverName = "http://192.168.1.111:5000/sensor_data"; // URL del server Flask

  // Configurazione MQTT
  const char* mqtt_server = "192.168.1.111"; // Indirizzo IP del broker MQTT
  const int mqtt_port = 1883;               // Porta del broker MQTT
  const char* mqtt_topic_sampling_rate = "iot/bed_alarm/sampling_rate"; // Topic per ricevere il nuovo sampling_rate
  const char* mqtt_topic_trigger_alarm = "iot/bed_alarm/trigger_alarm"; // Topic per ricevere il nuovo sampling_rate
  const char* mqtt_topic_stop_alarm = "iot/bed_alarm/stop_alarm"; // Topic per ricevere il nuovo sampling_rate
  const char* mqtt_topic_alarm_sound = "iot/bed_alarm/alarm_sound"  ; // Topic per ricevere il nuovo alarm_sound

  WiFiClient espClient;          // Client WiFi
  PubSubClient client(espClient); // Oggetto MQTT

  // Variabili globali
  // unsigned long last_sample_time = 0;  // Memorizza il timestamp dell'ultima lettura
  // unsigned long sampling_rate = 5000; // Intervallo tra le letture (in millisecondi)
  bool alert_active = false;          // Stato attuale dell'allarme (LED e speaker)

  // Variabili globali per l'accumulo dei dati
  unsigned long last_sample_time = 0;  // Timestamp dell'ultimo invio dei dati
  unsigned long sampling_rate = 5000;  // Intervallo in millisecondi per il calcolo della media
  long pressureSum = 0;              // Somma cumulativa delle letture
  unsigned int pressureCount = 0;    // Numero di letture accumulate
  unsigned int alarm_sound = 1; 

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

    // Legge il valore dal sensore ad ogni iterazione
    int sensorValue = analogRead(PRESSURE_SENSOR_PIN);
    // Serial.print("Valore del sensore: ");
    // Serial.println(sensorValue);

    // Accumula i valori
    pressureSum += sensorValue;
    pressureCount++;

    // Se è trascorso l'intervallo di sampling_rate, calcola la media
    if (current_time - last_sample_time >= sampling_rate) {
      float pressureAverage = pressureSum / (float)pressureCount;
      Serial.print("Media dei valori: ");
      Serial.println(pressureAverage);

      // Gestisce il LED e lo speaker in base al valore medio
      handleAlert(pressureAverage);

      // Invia i dati al server Flask (modifica il payload se necessario)
      sendPressureData(pressureAverage);

      // Resetta i contatori per il prossimo intervallo
      pressureSum = 0;
      pressureCount = 0;
      last_sample_time = current_time;
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
        client.subscribe(mqtt_topic_sampling_rate);
        // Sottoscrive ai topic per attivare l'allarme
        client.subscribe(mqtt_topic_trigger_alarm);
        // Sottoscrive ai topic per disattivare l'allarme
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



    if (alert_active && pressureValue >= PRESSURE_THRESHOLD) {
      digitalWrite(LED_BUILTIN, HIGH); // Accende il LED
      playMelody(alarm_sound);     // Suona lo speaker a 2000 Hz
    } else {
      digitalWrite(LED_BUILTIN, LOW);  // Spegne il LED
      noTone(SPEAKER_PIN);             // Ferma lo speaker
      alert_active = false;            // Disattiva lo stato di allarme
    }
  }

  void playMelody(int soundType) {
    switch (soundType) {
        case 1: // Clear (Soleggiato) - Energico
            tone(SPEAKER_PIN, 1000, 200);
            delay(250);
            tone(SPEAKER_PIN, 1200, 200);
            delay(250);
            tone(SPEAKER_PIN, 1400, 200);
            delay(250);
            tone(SPEAKER_PIN, 1000, 200);
            delay(250);
            tone(SPEAKER_PIN, 1200, 200);
            delay(250);
            tone(SPEAKER_PIN, 1400, 200);
            delay(250);
            tone(SPEAKER_PIN, 1000, 200);
            delay(250);
            tone(SPEAKER_PIN, 1200, 200);
            delay(250);
            tone(SPEAKER_PIN, 1400, 200);
            delay(250);
            tone(SPEAKER_PIN, 1000, 200);
            delay(250);
            tone(SPEAKER_PIN, 1200, 200);
            delay(250);
            tone(SPEAKER_PIN, 1400, 200);
            delay(250);
            Serial.print("Sta suonando il suono 1");
            break;

        case 2: // Clouds (Nuvoloso) - Medio
            tone(SPEAKER_PIN, 800, 300);
            delay(350);
            tone(SPEAKER_PIN, 900, 300);
            tone(SPEAKER_PIN, 800, 300);
            delay(350);
            tone(SPEAKER_PIN, 900, 300);
            tone(SPEAKER_PIN, 800, 300);
            delay(350);
            tone(SPEAKER_PIN, 900, 300);
            tone(SPEAKER_PIN, 800, 300);
            delay(350);
            tone(SPEAKER_PIN, 900, 300);
            tone(SPEAKER_PIN, 800, 300);
            delay(350);
            tone(SPEAKER_PIN, 900, 300);
            tone(SPEAKER_PIN, 800, 300);
            delay(350);
            tone(SPEAKER_PIN, 900, 300);
            delay(350);
            break;

        case 3: // Rain/Drizzle (Pioggia/Piovigginoso) - Rilassante
            tone(SPEAKER_PIN, 500, 400);
            delay(450);
            tone(SPEAKER_PIN, 600, 400);
            delay(450);
            break;

        case 4: // Thunderstorm (Temporale) - Allerta
            tone(SPEAKER_PIN, 2000, 100);
            delay(150);
            tone(SPEAKER_PIN, 2500, 100);
            delay(150);
            tone(SPEAKER_PIN, 3000, 100);
            delay(150);
            tone(SPEAKER_PIN, 1500, 300);
            delay(350);
            break;

        default:
            tone(SPEAKER_PIN, 1000, 200);
            delay(250);
            tone(SPEAKER_PIN, 1200, 200);
            delay(250);
            tone(SPEAKER_PIN, 1400, 200);
            delay(250);
            break;
            break;
    }
}
