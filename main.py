import machine
import utime
import random

# --------------------------------------------------
# Pin configuration on ESP32 , https://www.amazon.ca/dp/B0D8T53CQ5?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1
# Original repo:
#   switchPin = 15
#   ledPin    = 14
#   motorPin  = 13
#
# Change these if your wiring differs.
# --------------------------------------------------
SWITCH_PIN = 15
LED_PIN = 14
SERVO_PIN = 13

# --------------------------------------------------
# Servo calibration from the original sketch
# You may need to tweak these for your build.
# --------------------------------------------------
RESTING_POS = 160
PEEKING_POS = 80
INTERMEDIATE_POS = 60
PUSHING_POS = 41

# Debounce and timing
DEBOUNCE_MS = 100
LOOP_DELAY_MS = 20
RESET_FIRST_ACTION_AFTER_MS = 5 * 60 * 1000  # 5 minutes

# --------------------------------------------------
# Hardware setup
# --------------------------------------------------
switch_pin = machine.Pin(SWITCH_PIN, machine.Pin.IN, machine.Pin.PULL_UP)
led = machine.Pin(LED_PIN, machine.Pin.OUT)

servo = machine.PWM(machine.Pin(SERVO_PIN))
servo.freq(50)  # standard servo frequency


# --------------------------------------------------
# Servo helpers
# --------------------------------------------------
def interval_mapping(x, in_min, in_max, out_min, out_max):
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min


def servo_write(angle):
    """
    Write an angle in degrees to the servo.
    Uses the same mapping style as your test script.
    """
    angle = max(0, min(180, int(angle)))
    pulse_width_ms = interval_mapping(angle, 0, 180, 0.5, 2.5)
    duty = int(interval_mapping(pulse_width_ms, 0, 20, 0, 65535))
    servo.duty_u16(duty)


def move_servo_slow(from_angle, to_angle, delay_ms):
    step = 1 if to_angle > from_angle else -1
    angle = from_angle
    while angle != to_angle:
        servo_write(angle)
        utime.sleep_ms(delay_ms)
        angle += step
    servo_write(to_angle)


# --------------------------------------------------
# LED helper
# --------------------------------------------------
led_on = False


def set_led(on):
    global led_on
    led.value(1 if on else 0)
    led_on = on


# --------------------------------------------------
# Actions translated from the Arduino sketch
# --------------------------------------------------
def simple():
    servo_write(PUSHING_POS)
    utime.sleep_ms(250)
    servo_write(RESTING_POS)


def slow():
    move_servo_slow(RESTING_POS, PUSHING_POS, 10)
    servo_write(INTERMEDIATE_POS)
    utime.sleep_ms(100)
    servo_write(PUSHING_POS)
    utime.sleep_ms(100)
    servo_write(RESTING_POS)


def very_slow():
    move_servo_slow(RESTING_POS, PUSHING_POS, 30)
    servo_write(INTERMEDIATE_POS)
    utime.sleep_ms(100)
    servo_write(PUSHING_POS)
    utime.sleep_ms(150)
    servo_write(RESTING_POS)


def slow_peek():
    move_servo_slow(RESTING_POS, PEEKING_POS, 10)
    servo_write(PEEKING_POS)
    utime.sleep_ms(500)
    servo_write(PUSHING_POS)
    utime.sleep_ms(250)
    servo_write(RESTING_POS)


def angry():
    set_led(True)
    for _ in range(3):
        servo_write(PUSHING_POS)
        utime.sleep_ms(250)
        servo_write(PEEKING_POS)
        utime.sleep_ms(200)
    servo_write(RESTING_POS)
    utime.sleep_ms(200)


def peeking():
    servo_write(PEEKING_POS)
    set_led(True)
    utime.sleep_ms(250)
    servo_write(PUSHING_POS)
    utime.sleep_ms(250)
    servo_write(RESTING_POS)
    utime.sleep_ms(200)


def peek_afterwards():
    set_led(True)
    utime.sleep_ms(100)
    servo_write(PEEKING_POS)
    utime.sleep_ms(1500)
    servo_write(RESTING_POS)


# Weighted action list, matching the original sketch's intent
ACTIONS = [
    simple,
    slow,
    very_slow,
    slow_peek,
    angry,
    peeking,
    simple,
    simple,
    simple,
    simple,
    slow_peek,
    peeking,
    peeking,
    angry,
    angry,
    angry,
]

# --------------------------------------------------
# Debounce state
# --------------------------------------------------
last_switch_state = switch_pin.value()
stable_switch_state = last_switch_state
last_debounce_time = utime.ticks_ms()


def update_debounced_switch():
    """
    Returns the stable debounced switch value.
    Assumes active LOW switch like the Arduino sketch.
    """
    global last_switch_state, stable_switch_state, last_debounce_time

    reading = switch_pin.value()

    if reading != last_switch_state:
        last_debounce_time = utime.ticks_ms()
        last_switch_state = reading

    if utime.ticks_diff(utime.ticks_ms(), last_debounce_time) > DEBOUNCE_MS:
        stable_switch_state = reading

    return stable_switch_state


# --------------------------------------------------
# Main loop state
# --------------------------------------------------
last_action_ms = None
first_action = True

# Initial position like setup()
set_led(False)
servo_write(RESTING_POS)
utime.sleep_ms(500)

# Seed RNG roughly like the Arduino code
random.seed(utime.ticks_cpu())


# --------------------------------------------------
# Main loop
# --------------------------------------------------
while True:
    switch_state = update_debounced_switch()

    # Active LOW like original getState() == LOW
    if switch_state == 0:
        now = utime.ticks_ms()

        if last_action_ms is None:
            since_last_action = RESET_FIRST_ACTION_AFTER_MS + 1
        else:
            since_last_action = utime.ticks_diff(now, last_action_ms)

        if since_last_action > RESET_FIRST_ACTION_AFTER_MS:
            first_action = True

        if first_action:
            action = ACTIONS[0]   # always simple on first use
        else:
            action = random.choice(ACTIONS)

        action()

        # Same spirit as original: sometimes do an extra peek if used again quickly
        if (not first_action) and (since_last_action < 5000):
            if random.randint(1, 9) > 6:
                peek_afterwards()

        last_action_ms = utime.ticks_ms()
        first_action = False

        # Wait for switch release so one flip triggers once
        while update_debounced_switch() == 0:
            utime.sleep_ms(10)

    if led_on:
        set_led(False)

    utime.sleep_ms(LOOP_DELAY_MS)