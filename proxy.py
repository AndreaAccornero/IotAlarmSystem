from datetime import datetime
import logging
import random
import threading
import time
from flask import Flask, request, jsonify, render_template
from influxdb_client import InfluxDBClient, Point, WriteOptions
import paho.mqtt.client as mqtt
from dotenv import load_dotenv
import os
import json

from utils import get_weather_data, load_alarms_from, save_alarms_to

# Carica le variabili d'ambiente
load_dotenv(".env")

# Configurazione API_KEY OpenWeatherMap
WEATHER_API_KEY = os.getenv("WEATHER_API_KEY")

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
mqtt_topic_alarm_sound = "iot/bed_alarm/alarm_sound"  
mqtt_topic_trigger_alarm = "iot/bed_alarm/trigger_alarm"

# Nuovi topic MQTT per il nuovo allarme e la location
# mqtt_topic_new_alarm = "iot/bed_alarm/new_alarm"
# mqtt_topic_location = "iot/bed_alarm/location_alarm"

# Variabili globali
sampling_rate = 15  # Intervallo tra le letture consecutive
stop_alarm = False
alarm_sound = "sound1"  # Valore di default per l'allarme
weather_location = "Bologna"  # Location di default
alarm_filename = "alarms.json" # Nome del file per salvare gli allarmi
alarms = []  # Lista degli allarmi

# Funzione per generare i dati da scrivere su InfluxDB
def generate_data(pressure_value):
    return [
        {"measurement": "pressure", "fields": {"value": pressure_value}},
    ]

# Callback MQTT
def on_connect(client, userdata, flags, rc):
    # Iscriviti al topic principale
    client.subscribe("iot/bed_alarm")

mqtt_client = mqtt.Client(f"PythonClient-{random.randint(1000, 9999)}")
mqtt_client.on_connect = on_connect
mqtt_client.connect(mqtt_broker, mqtt_port, 60)
mqtt_client.loop_start()

# Inizializza Flask
app = Flask(__name__)

# def publish_with_retry(client, topic, message, max_retries=20, retry_delay=0.5):
#     attempt = 0
#     while attempt < max_retries:
#         if client.is_connected():
#             client.loop()
#             result = client.publish(topic, json.dumps(message))
#             if result.rc == 0:
#                 print(f"Messaggio pubblicato con successo su {topic}")
#                 return True
#             else:
#                 print(f"Errore nella pubblicazione su {topic}, codice {result.rc}. Tentativo {attempt+1}/{max_retries}")
#         else:
#             print("Errore: MQTT Client disconnesso. Riprovo...")

#         time.sleep(retry_delay)
#         attempt += 1

#     print(f"Errore: impossibile pubblicare su {topic} dopo {max_retries} tentativi.")
#     return False   

# Endpoint per aggiornare il sampling rate
@app.route('/update_sampling_rate', methods=['POST'])
def update_sampling_rate():
    global sampling_rate
    data = request.json
    if 'sampling_rate' in data:
        sampling_rate = int(data['sampling_rate'])
        mqtt_client.publish(mqtt_topic_sampling_rate, json.dumps({"sampling_rate": sampling_rate}))

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
        # Confronta la stringa "true" per aggiornare la variabile booleana
        stop_alarm = data['stop_alarm'] == "true"
        mqtt_client.publish(mqtt_topic_stop_alarm, json.dumps({"stop_alarm": stop_alarm}))
        print(f"Published stop_alarm: {stop_alarm} to {mqtt_topic_stop_alarm}")
    
    print("------ Updated Stop Alarm ------")
    print(f"Stop Alarm: {stop_alarm}")
    print("--------------------------------")
    
    return jsonify({"stop_alarm": stop_alarm, "status": "success"})

# Endpoint per aggiornare l'alarm sound
@app.route('/update_alarm_sound', methods=['POST'])
def update_alarm_sound():
    global alarm_sound
    data = request.json
    if 'alarm_sound' in data:
        # Utilizza direttamente il valore stringa inviato dall'HTML ("sound1", "sound2", "sound3")
        alarm_sound = data['alarm_sound']

        mqtt_client.publish(mqtt_topic_alarm_sound, json.dumps({"alarm_sound": alarm_sound}))
        print(f"Published alarm_sound: {alarm_sound} to {mqtt_topic_alarm_sound}")
    
    print("------ Trigger alarm_sound ------")
    print(f"Alarm Sound: {alarm_sound}")
    print("-----------------------------------")
    
    return jsonify({"alarm_sound": alarm_sound, "status": "success"})

# Endpoint per settare un nuovo allarme (data, orario, frequenza)
@app.route('/set_new_alarm', methods=['POST'])
def set_new_alarm():
    data = request.json
    global alarms

    alarm_id = data.get("alarm_id")
    alarm_time = data.get("alarm_time")
    alarm_frequency = data.get("alarm_frequency")

    # Controllo se alarm_id è già presente
    if any(alarm['alarm_id'] == alarm_id for alarm in alarms):
        return jsonify({"status": "error", "message": "Alarm ID already exists"}), 400

    # Controllo se alarm_time è presente e valido
    if not alarm_time:
        return jsonify({"status": "error", "message": "Missing required fields"}), 400

    alarm = {
        "alarm_id": alarm_id,
        "alarm_time": alarm_time,
        "alarm_frequency": alarm_frequency,
        "active": True
    }
    print(alarm)

    alarms.append(alarm)
    save_alarms_to(alarm_filename, alarms)

    return jsonify({"status": "success", "message": "Alarm set successfully"}), 201

    
@app.route('/alarms', methods=['GET'])
def get_alarms():
    global alarms
    return jsonify({"alarms": alarms}), 200  # Assicuriamoci che ritorni sempre un JSON valido

# Endpoint per modificare una sveglia
@app.route('/update_alarm/<alarm_id>', methods=['PUT'])
def modify_alarm(alarm_id):
    '''
    Modifies the properties of an alarm.
    '''
    data = request.json
    global alarms

    for alarm in alarms:
        if alarm["alarm_id"] == alarm_id:
            alarm["alarm_time"] = data.get("alarm_time", alarm["alarm_time"])
            alarm["alarm_frequency"] = data.get("alarm_frequency", alarm["alarm_frequency"])
            alarm["active"] = data.get("active", str(alarm["active"])).lower() == "true"  # Convert to boolean
            save_alarms_to(alarm_filename, alarms)
            return jsonify({"message": "Alarm updated successfully", "alarm": alarm}), 201

    return jsonify({"error": "Alarm not found"}), 404

# Endpoint per eliminare una sveglia
@app.route('/remove_alarm/<alarm_id>', methods=['DELETE'])
def remove_alarm(alarm_id):
    '''
    Deletes an alarm.
    '''
    global alarms

    len1 = len(alarms)
    alarms = [alarm for alarm in alarms if alarm["alarm_id"] != alarm_id]
    if len(alarms) == len1:
        return jsonify({"message": "Alarm was not deleted."}), 400
    save_alarms_to(alarm_filename, alarms)  # Save to file after deletion
    return jsonify({"message": "Alarm deleted successfully"}), 201

# Endpoint per eliminare tutte le sveglie
@app.route('/remove_all_alarms', methods=['DELETE'])
def remove_all_alarms():
    '''
    Deletes all alarms.
    '''
    global alarms
    alarms = []
    save_alarms_to(alarm_filename, alarms)  # Save to file after deletion
    return jsonify({"message": "All alarms deleted successfully"}), 201

# Endpoint per settare la location della sveglia
@app.route('/set_alarm_location', methods=['POST'])
def set_alarm_location():
    data = request.json
    global weather_location

    # Se non viene fornita una location, utilizza quella di default
    location = data.get("location", weather_location)
    # mqtt_client.publish(mqtt_topic_location, json.dumps({"location": location}))
    # mqtt_client.loop(2)
    # print(f"Published alarm location: {location} to {mqtt_topic_location}")
    
    return jsonify({"location": location, "status": "success"}), 201

# @app.route('/getWeather', methods=['GET'])
# def get_weather():
#     '''
#     Get current weather.
#     '''
#     global weather_location

#     weather_data =
#     if weather_data:
#         return json.dumps({"weather": weather_data}), 200
#     return jsonify({"error": "Weather server unavailable"}), 503

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
    return "OK", 201

def get_weather_conditions():
    """
    Gets the weather conditions for the current location.
    """
    global weather_location, WEATHER_API_KEY

    if WEATHER_API_KEY:
        if weather_location:
            weather_condition = get_weather_data(weather_location, WEATHER_API_KEY)
            sound_mapping = {
                "Clear": "sound1",  # Soleggiato = Energico
                "Clouds": "sound2",  # Nuvoloso = Medio
                "Rain": "sound3",  # Pioggia = Rilassante
                "Drizzle": "sound3",  # Piovigginoso = Rilassante
                "Snow": "sound4",  # Neve = Dolce
                "Thunderstorm": "sound5"  # Temporale = Allerta
            }

            selected_sound = sound_mapping.get(weather_condition, "sound1")  # Default: sound1
            print(f"Condizione meteo: {weather_condition}, Suono selezionato: {selected_sound}")
            return selected_sound
        
    return "sound1"  # Default sound

# ----- Alarm clock thread -----
def alarm_clock():
    """
    Runs in a separate thread, continuously checks for active alarms and triggers them.
    """
    logging.info("Alarm clock manager thread ready.")
    global alarms, alarm_triggered, mqtt_client, mqtt_topic_alarm_sound, mqtt_topic_trigger_alarm

    saved_time = ""

    while True:
        now = datetime.now()
        current_time = now.strftime("%H:%M")  # format: HH:MM
        current_weekday = now.weekday()  # weekdays: 0 (Monday) - 6 (Sunday)

        for alarm in alarms:
            alarm_time = alarm.get("alarm_time")
            alarm_active = alarm.get("active", False)
            alarm_frequency = alarm.get("alarm_frequency", "once")

            if alarm_active and alarm_time == current_time:
                # Controllo della frequenza dell'allarme
                should_trigger = False
                
                if alarm_frequency == "everyday":
                    should_trigger = True
                elif alarm_frequency == "weekdays" and current_weekday < 5:
                    should_trigger = True
                elif alarm_frequency == "weekends" and current_weekday >= 5:
                    should_trigger = True
                elif alarm_frequency == "once" and saved_time != current_time:
                    should_trigger = True
                    alarm["active"] = False  # Disattiva l'allarme dopo il trigger once
                    save_alarms_to(alarm_filename, alarms)
                elif alarm_frequency.startswith("every_"):
                    # Mappa per controllare i giorni della settimana
                    weekdays_map = {
                        "every_monday": 0,
                        "every_tuesday": 1,
                        "every_wednesday": 2,
                        "every_thursday": 3,
                        "every_friday": 4,
                        "every_saturday": 5,
                        "every_sunday": 6
                    }
                    if weekdays_map.get(alarm_frequency) == current_weekday:
                        should_trigger = True

                if should_trigger and saved_time != current_time:
                    saved_time = current_time
                    # Attempt to get weather data
                    weather_sound = get_weather_conditions()
                    mqtt_client.publish(mqtt_topic_alarm_sound, json.dumps({"alarm_sound": weather_sound}))
                    mqtt_client.publish(mqtt_topic_trigger_alarm, json.dumps({"trigger_alarm": "trigger_alarm"}))

                    print(f"Alarm {alarm.get('alarm_id', 'unknown')} triggered at {current_time} on weekday {current_weekday}")

        time.sleep(10)  # check every 10 seconds, to make it less expensive


# Pagina HTML principale
@app.route('/')
def index():
    return render_template('index.html')

if __name__ == "__main__":

    # Notify ESP32 about broker IP in a separate thread
    alarms = load_alarms_from(alarm_filename)

    # set the alarm clock thread
    alarm_thread = threading.Thread(target=alarm_clock, daemon=True)
    alarm_thread.start()

    # start Flask app/backend server
    app.run(host="0.0.0.0", port=5000, debug=True, threaded=True)
