This device is designed to connect to the serial port of the WiFi dongle attached to a SolArk inverter. (Pictured below is that dongle with the plastic housing removed and three wires -- white: receive, black: ground, green: transmit -- attached to its serial port.) Doing this allows it to intercept the important data (battery charge percentage, battery drain & charge wattage, load wattage, solar wattage, and grid wattage) at fine granularity (every ten seconds or so).

![alt text](dongle_serial.jpg?raw=true)

Since this device also has the ability to control devices, the plan is to use it to turn on a nearby Generac generator and perhaps also send signals to the SolArk inverter itself (assuming this can be done).

For a backend, this device uses the same one used by the ESP Remote.  See:

https://github.com/judasgutenberg/Esp8266_RemoteControl
