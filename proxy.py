from flask import Flask, request
from influxdb_client import InfluxDBClient, Point, WriteOptions
import paho.mqtt.client as mqtt
from dotenv import load_dotenv 
import os
import time 
import json

load_dotenv(".env")

# Configurazione di InfluxDB
token = os.getenv("token")
org = "IoTAlarmSystem"
bucket = "Prova"

client = InfluxDBClient(url="http://localhost:8086", token=token, org=org)
write_api = client.write_api(write_options=WriteOptions(batch_size=1))

mqtt_broker = "localhost"
mqtt_port = 1883
mqtt_topic_data = "iot/bed_alarm/update_sampling_rate"

sampling_rate = 15 # Intervallo tra le letture dei sensori (secondi)

# MQTT Callbacks
def on_connect(client, userdata, flags, rc):
    print(f"Connected to MQTT broker with result code {rc}")
    client.subscribe(mqtt_topic_data)


def on_message(client, userdata, msg):
    print(f"Messaggio ricevuto su topic {msg.topic}: {msg.payload.decode()}")
    # global sampling_rate
    # payload = msg.payload.decode('utf-8')
    # print(f"Received message '{payload}' on topic '{msg.topic}'")
    # try:
    #     command = json.loads(payload)
    #     if 'sampling_rate' in command:
    #         sampling_rate = int(command['sampling_rate'])
    #         print(f"Updated sampling rate to {sampling_rate} seconds")
            
    # except json.JSONDecodeError:
    #     print("Invalid command format")


# Inizializza il client MQTT
mqtt_client = mqtt.Client("PythonClient")
mqtt_client.connect(mqtt_broker, mqtt_port, 60)
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.loop_start()

mqtt_client.publish(mqtt_topic_data, json.dumps({"sampling_rate": sampling_rate}))
mqtt_client.loop_stop()

def generate_data(pressure_value):
    # Puoi modificare questa funzione per generare dati realistici
    return [
        {"measurement": "pressure",  "fields": {"value": pressure_value}},
    ]

# Configurazioni
# Inizializza l'app Flask per la gestione delle richieste HTTP
app = Flask(__name__)

# Flask endpoint per ricevere i dati dal sensore
@app.route('/sensor_data', methods=['POST'])
def sensor_data():
    data = request.json
    if data and 'pressure_value' in data:
        pressure_value = data['pressure_value']
        print(f"Received pressure value: {pressure_value}")

        tmp = generate_data(pressure_value)
        
        for point in tmp:
            try:
                write_api.write(bucket=bucket, record=Point.from_dict(point))
                print(f"Dato scritto: {point}")
                # time.sleep(2)  # Aggiungi un ritardo di 5 secondi
            except Exception as e:
                print(f"Errore durante la scrittura dei dati: {e}")

    return "OK", 200

# Chiudi la connessione a InfluxDB
# client.close()

# Avvia il server Flask per ricevere i dati dal sensore
if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000, debug=True)
    # Pubblica il nuovo sampling rate per Arduino
    # mqtt_client.publish(mqtt_topic_data, json.dumps({"sampling_rate": sampling_rate}))

