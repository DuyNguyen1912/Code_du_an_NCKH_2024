# Dự án điều khiển các thiết bị trong nhà thông minh bằng cách nhắn tin với ChatBot 
# Các thiết bị gồm: Đèn, quạt, bình nóng lạnh, điều hòa, cửa chính.
import json
import time
import Adafruit_DHT
from flask import Flask, request, make_response
import RPi.GPIO as GPIO

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)
GPIO.setup(18, GPIO.OUT) # LED 1
GPIO.setup(23, GPIO.OUT) # LED 2
GPIO.setup(24, GPIO.OUT) # LED 3
GPIO.setup(25, GPIO.OUT) # led 4

GPIO.setup(21, GPIO.OUT) #fan

# Khai báo chân GPIO cho servo
servo_pin = 20
GPIO.setup(servo_pin, GPIO.OUT)
pwm = GPIO.PWM(servo_pin, 50)
pwm.start(0)
# Hàm để đặt góc quay của servo
def set_angle(angle):
    duty = angle / 20 + 2
    GPIO.output(servo_pin, True)
    pwm.ChangeDutyCycle(duty)
    time.sleep(1)
    GPIO.output(servo_pin, False)
    pwm.ChangeDutyCycle(0)

app = Flask(__name__)

@app.route('/on1')
def turn_on1():
    GPIO.output(18, GPIO.HIGH) # Bật LED 1
    # Tắt LED 2 (nếu cần)
    GPIO.output(23, GPIO.LOW)
    return make_response(json.dumps({"status": "on"}), 200)

@app.route('/off1')
def turn_off1():
    GPIO.output(18, GPIO.LOW) # Tắt LED 1
    return make_response(json.dumps({"status": "off"}), 200)

@app.route('/on2')
def turn_on2():
    GPIO.output(23, GPIO.HIGH) # Bật LED 2
    # Tắt LED 1 (nếu cần)
    GPIO.output(18, GPIO.LOW)
    return make_response(json.dumps({"status": "on"}), 200)

@app.route('/off2')
def turn_off2():
    GPIO.output(23, GPIO.LOW) # Tắt LED 2
    return make_response(json.dumps({"status": "off"}), 200)
# led 3
@app.route('/on3')
def turn_on3():
    GPIO.output(24, GPIO.HIGH) # Bật LED 3
    
    return make_response(json.dumps({"status": "on"}), 200)

@app.route('/off3')
def turn_off3():
    GPIO.output(24, GPIO.LOW) # Tắt LED 3
    return make_response(json.dumps({"status": "off"}), 200)
# led4
@app.route('/on4')
def turn_on4():
    GPIO.output(25, GPIO.HIGH) # Bật led4
    return make_response(json.dumps({"status": "on"}), 200)

@app.route('/off4')
def turn_off4():
    GPIO.output(25, GPIO.LOW) # Tắt led4
    return make_response(json.dumps({"status": "off"}), 200)
# quat 
@app.route('/onfan2')
def turn_onfan2():
    GPIO.output(21, GPIO.HIGH) # Bật quat
    # 
    return make_response(json.dumps({"status": "on"}), 200)

@app.route('/off-fan2')
def turn_off_fan2():
    GPIO.output(21, GPIO.LOW) # Tắt quat
    return make_response(json.dumps({"status": "off"}), 200)
# door
@app.route('/open_door')
def open_door():
    return make_response(json.dumps({"status": "open"}), 200)

@app.route('/close_door')
def close_door():
    return make_response(json.dumps({"status": "close"}), 200)

@app.route('/status')
def get_status():
    status1 = "on" if GPIO.input(18) else "off"
    status2 = "on" if GPIO.input(23) else "off"
    status3 = "on" if GPIO.input(24) else "off"
    status4 = "on" if GPIO.input(25) else "off"
   

    return make_response(json.dumps({"status1": status1, "status2": status2, "status3": status3, "status4": status4}), 200)
# status cho fan
@app.route('/status2')
def get_status2():
     status5 = "on" if GPIO.input(21) else "off"
     return make_response(json.dumps({"status5": status5}), 200)
# Nhiet do
@app.route('/temperature')
def get_temperature():
    unit = request.args.get('unit', default='C', type=str)
    humidity, temperature = Adafruit_DHT.read_retry(Adafruit_DHT.DHT11, 27)
    if humidity is not None and temperature is not None:
        if unit.upper() == 'F':
            temperature = temperature * 1.8 + 32
            unit_display = '°F'
        else:
            unit_display = '°C'
        return make_response(json.dumps({"temperature": temperature, "unit": unit_display}), 200)
    else:
        return make_response(json.dumps({"error": "Failed to retrieve temperature data"}), 500)
#
@app.route('/webhook', methods=['POST'])
def webhook():
    req = request.get_json(silent=True, force=True)
    action = req.get("queryResult", {}).get("action", "")
    print("Request:")
    print(json.dumps(req, indent=4))
    res = makeWebhookResult(req)
    res = json.dumps(res, indent=4)
    print("Response:")
    print(res)
    return make_response(res)

def makeWebhookResult(req):
    speech = ""  # Khởi tạo speech với một chuỗi rỗng
    action = req.get("queryResult", {}).get("action", "")

    if action == "temperature":
        _ , temperature = Adafruit_DHT.read_retry(Adafruit_DHT.DHT11, 27)
        if temperature is not None:
            speech = f"Temperature is {temperature}°C"
        else:
            speech = "Failed to retrieve temperature data"
    elif action == "humidity":
        humidity, _ = Adafruit_DHT.read_retry(Adafruit_DHT.DHT11, 27)
        if humidity is not None:
            speech = f"Humidity is {humidity}%"
        else:
            speech = "Failed to retrieve humidity data"
    elif action == "dht11":
        humidity, temperature = Adafruit_DHT.read_retry(Adafruit_DHT.DHT11, 27)
        if humidity is not None and temperature is not None:
            speech = f"Temperature is {temperature}°C and Humidity is {humidity}%"
        else:
            speech = "Failed to retrieve temperature and humidity data"
# kiem tra neu hanh dong la open
    elif action == "open-close":
        open_close_action = req.get("queryResult", {}).get("parameters", {}).get("open-close", [])[0]
        if open_close_action == "open":
            # Thực hiện hành động mở cửa
            for angle in range(180, -1, -180):
                set_angle(angle)
            open_door()
            speech = "ok, open door"
        elif open_close_action == "close":
            # Thực hiện hành động đóng cửa
            close_door()
            speech = "ok, close door"
            # Điều khiển servo từ 0 độ đến 180 độ
            for angle in range(0, 181, 180):
                set_angle(angle)
        else:
            speech = "Unknown action"

    elif action == "turn_on_led1":
        GPIO.output(18, GPIO.HIGH)  # Bật LED 1
        GPIO.output(23, GPIO.LOW)   # Tắt LED 2
        speech = "ok, on led1"
    elif action == "turn_off_led1":
        GPIO.output(18, GPIO.LOW)  # Tắt LED 1
        speech = "ok, off led1"
    # bat led 3
    elif action == "turn_on_led3":
        GPIO.output(24, GPIO.HIGH)  # Bật LED 3
        speech = "ok, on led3"
    elif action == "turn_off_led3":
        GPIO.output(24, GPIO.LOW)  # Tắt LED 3
        speech = "ok, off led3"
     # bat led 4
    elif action == "turn_on_led4":
        GPIO.output(25, GPIO.HIGH)  # Bật LED 4
        speech = "ok, on led4"
    elif action == "turn_off_led4":
        GPIO.output(25, GPIO.LOW)  # Tắt LED 4
        speech = "ok, off led4"
         # bat quat 
    elif action == "turn_on_fan":
        GPIO.output(21, GPIO.HIGH)  # Bật LED 4
        speech = "ok, on fan"
    elif action == "turn_off_fan":
        GPIO.output(21, GPIO.LOW)  # Tắt LED 4
        speech = "ok, off fan"
    elif action == "on-off":
        led_state = req.get("queryResult", {}).get("parameters", {}).get("on-off", [])[0]
        led_name = req.get("queryResult", {}).get("parameters", {}).get("led", [])[0]
        if led_state == "on":
            if led_name == "led1":
                GPIO.output(18, GPIO.HIGH)
            elif led_name == "led2":
                GPIO.output(23, GPIO.HIGH)
            speech = f"ok, on {led_name}"

        elif led_state == "off":
            if led_name == "led1":
                GPIO.output(18, GPIO.LOW)
            elif led_name == "led2":
                GPIO.output(23, GPIO.LOW)
            # elif led_name == "led3":
            #     GPIO.output(24, GPIO.HIGH)
            speech = f"ok, off {led_name}"

        if led_state == "on":
            if led_name == "led3":
                GPIO.output(24, GPIO.HIGH)
            elif led_name == "led4":
                GPIO.output(25, GPIO.HIGH)
            speech = f"ok, on {led_name}"
        elif led_state == "off":
            if led_name == "led3":
                GPIO.output(24, GPIO.LOW)
            elif led_name == "led4":
                GPIO.output(25, GPIO.LOW)
            speech = f"ok, off {led_name}"

    # ---------------------------------
        # else:
            # speech = "Unknown action"
        # if led_state == "on":#led3
        #     GPIO.output(24, GPIO.HIGH) if led_name == "led3" else GPIO.output(24, GPIO.HIGH)
        #     speech = f"ok, on {led_name}"
        # elif led_state == "off":
        #     GPIO.output(24, GPIO.LOW) if led_name == "led3" else GPIO.output(24, GPIO.LOW)
        #     speech = f"ok, off {led_name}" 
        # if led_state == "on":#led2
        #     GPIO.output(23, GPIO.HIGH) if led_name == "led3" else GPIO.output(24, GPIO.HIGH)
        #     speech = f"ok, on {led_name}"
        # elif led_state == "off":
        #     GPIO.output(23, GPIO.LOW) if led_name == "led3" else GPIO.output(24, GPIO.LOW)
        #     speech = f"ok, off {led_name}"
    # else:
    #     speech = "Unknown action"
    elif action == "on-off-fan":
        led_state_fan = req.get("queryResult", {}).get("parameters", {}).get("on-off-fan", [])[0]
        led_name_fan = req.get("queryResult", {}).get("parameters", {}).get("fan", [])[0]
        if led_state_fan == "turn on the":
            if led_name_fan == "fan":
                GPIO.output(21, GPIO.HIGH)
            # elif led_name == "led2":
            #     GPIO.output(23, GPIO.HIGH)
            # elif led_name == "led3":
            #     GPIO.output(24, GPIO.HIGH)
            speech = f"ok, on {led_name_fan}"
        elif led_state_fan == "turn off the":
            if led_name_fan == "fan":
                GPIO.output(21, GPIO.LOW)
    #        # fan fan fan
    # elif action == "on-off-fan":
    #     fan_state = req.get("queryResult", {}).get("parameters", {}).get("on-off-fan", [])[0]
    #     fan_name = req.get("queryResult", {}).get("parameters", {}).get("fan", [])[0]
    #     if fan_state == "on":
    #         GPIO.output(25, GPIO.HIGH) if fan_name == "fan1" else GPIO.output(9, GPIO.HIGH)
    #         speech = f"ok, on {fan_name}"
    #     elif fan_state == "off":
    #         GPIO.output(25, GPIO.LOW) if fan_name == "fan1" else GPIO.output(9, GPIO.LOW)
    #         speech = f"ok, off {fan_name}"
    #     else:
    #         speech = "Unknown action"
            # fan fan fan
    #door
    elif action == "open-close":
        door_state = req.get("queryResult", {}).get("parameters", {}).get("open-close", [])[0]
        door_name = req.get("queryResult", {}).get("parameters", {}).get("door", [])[0]
        if door_state == "open the":
            if door_name == "door":
                GPIO.output(20, GPIO.HIGH)
                speech = f"ok, open {door_name}"
        elif door_state == "close the":
            if door_name == "door":
                GPIO.output(20, GPIO.LOW)
                speech = f"ok, closed {door_name}"

    else:
        speech = "Unknown action"
    # 
    return {"fulfillmentText": speech}

    # 

@app.route('/', methods=['GET', 'POST'])
def index():
    if request.method == 'POST':
        payload = request.json
        user_response = payload.get("queryResult", {}).get("queryText", "")
        bot_response = payload.get("queryResult", {}).get("fulfillmentText", "")
        if user_response or bot_response:
            print("User: " + user_response)
            print("Bot: " + bot_response)
            return "Message received."
    else:
        return 'Chào mừng bạn đến với trang web của tôi!'

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
 