This device is designed to connect to the serial port of the WiFi dongle attached to a SolArk inverter. (Pictured below is that dongle with the plastic housing removed and three wires -- white: receive, black: ground, green: transmit -- attached to its serial port.) Doing this allows it to intercept the important data (battery charge percentage, battery drain & charge wattage, load wattage, solar wattage, and grid wattage) at fine granularity (every ten seconds or so).

![alt text](dongle_serial.jpg?raw=true)

In addition to being able to monitor a SolArk inverter via serial, it can also turn devices on and off via changes to pin state and regularly report weather data collected via sensors in the same way as the ESP8266s in my ESP8266 Remote Control system.
Since this device has the ability to control devices, I use it to remotely turn on a nearby Generac generator. Perhaps I can use it to send signals to the SolArk inverter itself (assuming this can be done).

For a backend, this device uses the same one used by the ESP Remote.  See:

https://github.com/judasgutenberg/Esp8266_RemoteControl
