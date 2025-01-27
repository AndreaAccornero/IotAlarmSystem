import paho.mqtt.client as mqtt
import requests
import random
import time
import json

# Configurazioni
mqtt_broker = "localhost"
mqtt_port = 1883
mqtt_topic_sampling_rate = "iot/bed_alarm/sampling_rate"
server_url = "http://localhost:5000/sensor_data"

# Variabili globali
sampling_rate = 10  

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker")
        client.subscribe(mqtt_topic_sampling_rate)
        print(f"Subscribed to topic: {mqtt_topic_sampling_rate}")
    else:
        print(f"Failed to connect, return code {rc}")

def on_message(client, userdata, msg):
    global sampling_rate
    print(f"Message received on topic {msg.topic}: {msg.payload.decode()}")
    try:
        payload = json.loads(msg.payload.decode())
        if "sampling_rate" in payload:
            new_rate = int(payload["sampling_rate"])
            if new_rate > 0:
                sampling_rate = new_rate
                print(f"Updated sampling rate to: {sampling_rate} seconds")
            else:
                print("Invalid sampling rate received")
    except json.JSONDecodeError as e:
        print(f"Failed to decode JSON: {e}")

def simulate_pressure_data():
    while True:
        # Genera un valore casuale per il sensore di pressione
        pressure_value = random.randint(0, 1023)
        print(f"Simulated Pressure Value: {pressure_value}")

        # Invia i dati al server Flask
        payload = {"pressure_value": pressure_value}
        try:
            response = requests.post(server_url, json=payload)
            if response.status_code == 200:
                print(f"Data sent successfully: {response.text}")
            else:
                print(f"Failed to send data: HTTP {response.status_code}")
        except requests.RequestException as e:
            print(f"Error sending data: {e}")

        # Rispetta il sampling rate
        time.sleep(sampling_rate)

if __name__ == "__main__":
    # Configura il client MQTT
    client = mqtt.Client("PythonSimulatedSensor")
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(mqtt_broker, mqtt_port, 60)
        client.loop_start()
        print("Starting simulated sensor...")

        # Simula i dati del sensore
        simulate_pressure_data()

    except KeyboardInterrupt:
        print("Exiting...")
        client.loop_stop()
        client.disconnect()
