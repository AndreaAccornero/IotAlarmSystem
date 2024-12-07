#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>  // Libreria MQTT
#include <ArduinoJson.h>   // Libreria per lavorare con JSON

// Definizione dei pin
const int speakerPin = 32;  // Pin GPIO collegato allo speaker
const int fsrPin = 35;    // Pin analogico collegato al FSR 
const int ledPin = 2;     // Pin collegato al LED integrato 

// Configurazione WiFi
#include <../credentials.h> // Credenziali WiFi
const char* mqtt_broker = "192.168.1.124";
const char* serverName = "http://192.168.1.124:5000/sensor_data";

WiFiClient espClient;
PubSubClient client(espClient);

// Variabile per memorizzare la lettura del FSR
int fsrValue = 0;

// Soglia di pressione per accendere il LED
int threshold = 1;  // Cambia questo valore in base alla sensibilità che desideri

// Frequenza di campionamento (default 5000 ms)
long sampling_rate = 5000;

void mqtt_callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  String msg;

  // Concatena il messaggio ricevuto
  for (int i = 0; i < length; i++) {
    msg += (char)message[i];
  }
  Serial.println("Message: " + msg);

  // Parsing del messaggio JSON per "iot/bed_alarm/update_sampling_rate"
  if (String(topic) == "iot/bed_alarm/update_sampling_rate") {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, msg);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.f_str());
      return;
    }

    if (doc.containsKey("sampling_rate")) {
      long new_sampling_rate = doc["sampling_rate"].as<long>() * 1000;  // Converti in millisecondi

      // Verifica che il valore sia valido
      if (new_sampling_rate > 0) {
        sampling_rate = new_sampling_rate;
        Serial.print("Updated sampling rate to: ");
        Serial.print(sampling_rate);
        Serial.println(" ms");
      } else {
        Serial.println("Invalid sampling rate received");
      }
    } else {
      Serial.println("JSON does not contain 'sampling_rate' key");
    }
  }

  // Parsing del messaggio JSON per "iot/bed_alarm/stop_alarm"
  else if (String(topic) == "iot/bed_alarm/stop_alarm") {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, msg);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.f_str());
      return;
    }

    if (doc.containsKey("stop_alarm")) {
      bool stop_alarm = doc["stop_alarm"].as<bool>();
      if (stop_alarm) {
        Serial.println("Stop alarm command received. Disabling alarm...");
        // Aggiungi qui la logica per fermare l'allarme
      } else {
        Serial.println("Stop alarm command received but value is false.");
      }
    } else {
      Serial.println("JSON does not contain 'stop_alarm' key");
    }
  }
}


void setup() {
  pinMode(speakerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Inizializza la comunicazione seriale per il debug
  Serial.begin(115200);

  // Connetti alla rete WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Configura il broker MQTT
  client.setServer(mqtt_broker, 1883);
  client.setCallback(mqtt_callback);

  // Connetti al broker MQTT
  connectToMQTT();
}

void connectToMQTT() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Connected to MQTT broker");

      // Iscriviti ai topic necessari
      client.subscribe("iot/bed_alarm/update_sampling_rate"); 
      Serial.println("Subscribed to topic: iot/bed_alarm/update_sampling_rate");

      client.subscribe("iot/bed_alarm/stop_alarm"); 
      Serial.println("Subscribed to topic: iot/bed_alarm/stop_alarm");
    } else {
      Serial.print("Failed MQTT connection, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 2 seconds");
      delay(2000);
    }
  }
}


void loop() {
  // Mantieni la connessione MQTT attiva e riconnettiti se necessario
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  // Legge il valore analogico dal FSR
  fsrValue = analogRead(fsrPin);

  // Stampa il valore del sensore per il debug
  Serial.print("FSR Value: ");
  Serial.println(fsrValue);

  // Se il valore supera la soglia, accendi il LED
  if (fsrValue > threshold) {
    digitalWrite(ledPin, LOW);   // Spegni il LED
    noTone(speakerPin);  
  } else {
    digitalWrite(ledPin, HIGH);  // Accendi il LED
    tone(speakerPin, 1000);   
  }

  // Invia i dati al server Flask
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

  Serial.print("Current sampling rate is: ");
  Serial.println(sampling_rate);
  Serial.println(stop_alarm)
  // Ritardo per evitare letture troppo frequenti
  delay(sampling_rate);
}
