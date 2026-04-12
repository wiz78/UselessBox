# UselessBox

Basic ESP32 firmware for my useless box

See `https://makerworld.com/en/models/76853` for the printable parts model.

**Implementations**
This repo contains two implementations:
- Arduino/C++ (original) in `UselessBox.ino`
- MicroPython in `main.py`

**Hardware Used (MicroPython Example)**
- Board: ELEGOO ESP-32 Dev Board (USB Type-C, 30-pin, CP2102). This board is slightly smaller than the original ESP32; a small shim was needed to fit the current 3D model.
- Servo: Micro Servo SG90
- Switch: Miniature Toggle Switch, SPST on/off, 2 terminals, 2 position

**Wiring (Text Diagram)**
Power and ground:
- VIN (5V) -> Servo V+
- GND -> Servo GND
- GND -> LED GND (split)
- GND -> Switch GND (split)

Signals:
- D13 -> Servo signal
- D14 -> 220 ohm resistor -> LED anode (+)
- D15 -> Switch signal (active LOW with internal pull-up)

Notes:
- Pins above match `main.py` for the MicroPython build.
- The switch is wired between D15 and GND, using the ESP32 internal pull-up.
