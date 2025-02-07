import os
import json
import logging
import requests

def load_alarms_from(alarm_file):
    '''
    Loads the alarm from a json file and returns them as a variable.
    '''
    global alarms
    if os.path.exists(alarm_file):
        try:
            with open(alarm_file, 'r') as file:
                alarms = json.load(file)
                logging.info(f"Loaded alarms from {alarm_file}.")
        except json.JSONDecodeError:
            logging.error(f"Failed to parse {alarm_file}. Starting with empty alarms.")
            alarms = []
    else:
        logging.info(f"{alarm_file} does not exist. Starting with empty alarms.")
        alarms = []
    return alarms

def save_alarms_to(alarm_file, alarms):
    '''
    Saves alarms to a json file.
    '''
    try:
        with open(alarm_file, 'w') as file:
            json.dump(alarms, file, indent=4)
            logging.info(f"Alarms saved to {alarm_file}.")
    except Exception as e:
        logging.error(f"Failed to save alarms to {alarm_file}: {e}")
    
def get_weather_data(city, WEATHER_API_KEY):
    """Ottiene la condizione meteo da OpenWeatherMap usando il nome della città."""
    api_url = f"http://api.openweathermap.org/data/2.5/weather?q={city}&appid={WEATHER_API_KEY}&units=metric"
    
    try:
        response = requests.get(api_url)
        if response.status_code == 200:
            payload = response.json()
            print(f"Weather API Response: {json.dumps(payload, indent=2)}")
            
            if "weather" in payload and len(payload["weather"]) > 0:
                return payload["weather"][0]["main"]
    except requests.RequestException as e:
        print(f"Error fetching weather data: {e}")
    
    return "Clear"  # Valore di default se la richiesta fallisce
