from flask import Flask, request, jsonify, render_template
from influxdb_client import InfluxDBClient, Point, WriteOptions
import paho.mqtt.client as mqtt
from dotenv import load_dotenv
import os
import json

# Carica le variabili d'ambiente
load_dotenv(".env")

# Configurazione di InfluxDB
token = os.getenv("token")
org = "IotAlarmSystem"
bucket = "Prova"

client = InfluxDBClient(url="http://localhost:8086", token=token, org=org)
write_api = client.write_api(write_options=WriteOptions(batch_size=1))

# Configurazione MQTT
mqtt_broker = "localhost"
mqtt_port = 1883
mqtt_topic_sampling_rate = "iot/bed_alarm/sampling_rate"
mqtt_topic_stop_alarm = "iot/bed_alarm/stop_alarm"
mqqtt_topic_alarm_sound = "iot/bed_alarm/alarm_sound"

# Variabili globali
sampling_rate = 15  # Intervallo tra le letture consecutive
stop_alarm = False

# Funzione per generare i dati
def generate_data(pressure_value):
    return [
        {"measurement": "pressure", "fields": {"value": pressure_value}},
    ]

# MQTT Callbacks
def on_connect(client, userdata, flags, rc):
    #print(f"Connected to MQTT broker with result code {rc}")
    client.subscribe("iot/bed_alarm")

def on_message(client, userdata, msg):
    print(f"Messaggio ricevuto su topic {msg.topic}: {msg.payload.decode()}")
    try:
        payload = json.loads(msg.payload.decode())
        global sampling_rate, stop_alarm
        if "sampling_rate" in payload:
            sampling_rate = int(payload["sampling_rate"])
            print(f"Updated sampling rate to {sampling_rate} seconds")
        if "stop_alarm" in payload:
            stop_alarm = payload["stop_alarm"]
            print(f"Stop alarm status updated to: {stop_alarm}")
    except json.JSONDecodeError:
        print("Invalid JSON format in MQTT message")

# Inizializza il client MQTT
mqtt_client = mqtt.Client("PythonClient")
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(mqtt_broker, mqtt_port, 60)
mqtt_client.loop_start()

# Inizializza Flask
app = Flask(__name__)

# Endpoint per aggiornare il sampling rate
@app.route('/update_sampling_rate', methods=['POST'])
def update_sampling_rate():
    global sampling_rate
    data = request.json
    if 'sampling_rate' in data:
        sampling_rate = int(data['sampling_rate'])
        mqtt_client.publish(mqtt_topic_sampling_rate, json.dumps({"sampling_rate": sampling_rate}))
        mqtt_client.loop(2)  # Forza il loop per inviare il messaggio
        print(f"Published sampling_rate: {sampling_rate} to {mqtt_topic_sampling_rate}")
    
    print("------ Updated Sampling Rate ------")
    print(f"Sampling Rate: {sampling_rate} seconds")
    print("-----------------------------------")
    
    return jsonify({"sampling_rate": sampling_rate, "status": "success"})


# Endpoint per aggiornare lo stato di stop_alarm
@app.route('/update_stop_alarm', methods=['POST'])
def update_stop_alarm():
    global stop_alarm
    data = request.json
    if 'stop_alarm' in data:
        stop_alarm = data['stop_alarm'] == "true"
        mqtt_client.publish(mqtt_topic_stop_alarm, json.dumps({"stop_alarm": stop_alarm}))
        mqtt_client.loop(2)  # Forza il loop per inviare il messaggio
        print(f"Published stop_alarm: {stop_alarm} to {mqtt_topic_stop_alarm}")
    
    print("------ Updated Stop Alarm ------")
    print(f"Stop Alarm: {stop_alarm}")
    print("--------------------------------")
    
    return jsonify({"stop_alarm": stop_alarm, "status": "success"})


# Endpoint to trigger alarm_sound
@app.route('/update_alarm_sound', methods=['POST'])
def update_alarm_sound():
    global alarm_sound
    data = request.json
    if 'alarm_sound' in data:
        alarm_sound = int(data['alarm_sound'])
        mqtt_client.publish(mqqtt_topic_alarm_sound, json.dumps({"alarm_sound": alarm_sound}))
        mqtt_client.loop(2)  # Forza il loop per inviare il messaggio
        print(f"Published alarm_sound: {alarm_sound} to {mqqtt_topic_alarm_sound}")
    
    print("------ Trigger alarm_sound ------")
    print(f"Alarm Sound: {alarm_sound}")
    print("-----------------------------------")
    
    return jsonify({"alarm_sound": alarm_sound, "status": "success"})



# Endpoint per ricevere i dati dal sensore
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
            except Exception as e:
                print(f"Errore durante la scrittura dei dati: {e}")
    return "OK", 200

# Pagina HTML principale
@app.route('/')
def index():
    return render_template('index.html')

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000, debug=True)
