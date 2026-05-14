

This describes a dedicated system for capturing and logging serial communications between a SolArk inverter and its WiFi dongle.  Originally this system was entirely hardcoded based on a careful analysis of the serial data.  That version can be found in 
solark_copilot_october2025.ino 

Later, though, SolArk changed their data format in an unexpected software update in October of 2025. This led me to build a generic serial stream parsing system and incorporated it into my ESP8266_Remote system (see https://github.com/judasgutenberg/Esp8266_RemoteControl). When configured with this certain data, it will automatically parse serial data arriving on the ESP8266's swapped serial RX pin and place it in the correct columns in the inverter_log table.


![alt text](dongle_serial.jpg?raw=true)


The first step in getting this to work is to carefully split open your SolArk WiFi dongle.  Inside there, you will find a labeled but unpopulated serial header.  This is a 3.3v serial port and its signals be connected directly to your ESP8266.  You do not have to solder pins into this header to connect to it.  I use  the swapped serial pins to keep interferences from the built-in USB interface on my board (a NodeMCU) from corrupting the data.  This means that the tx pin from the serial header on the WiFi dongle should connect to GPIO13, the rx pin on that header should be connected to GPIO15, and ground to ground.  Once this is set up, you need to flash a version of the ESP8266 Remote Control firmware from  https://github.com/judasgutenberg/Esp8266_RemoteControl.  While setting that up, you will need to specify some values correctly in config.cpp, particularly these:

 ```
  ci[SERIAL_SWAP] = 1; 
  ci[SERIAL_DEBUG_LEVEL] = 0; 
  ci[SERIAL_FOR_COMMANDS_ONLY] = 0; 
  ci[SERIAL_BUFFER] = 2048; 
  ci[SERIAL_PARSE_MODE] = 0; 
```

  Additionally, you will need to place a parse configuration file with the name serialparser.cfg in the root of a LittleFS file system on that ESP8266.  One way to do that is to fire up the flashed ESP8266 and send the command "download http://asecular.com/serialparser.cfg," which will download a known good copy to your ESP8266.  The contents of this file looks something like:

```
Characteristic #2;0x3ffbb61c;0x3ffbb5fc;4;5;6;7;8;9;10;11;2;3;0x3ffbb60c;0;1
Characteristic #7;I (318800102);0x3ffbb5bc;6;7
Characteristic #9;I (28318992);0x3ffbb64c;0;1
```
  
  

Then, when configuring the device with the assigned device_id (from config.cpp, whatever you set it for there) for this ESP8266, you should place a proper parser data disposition JSON object in its parsed data disposition. This is the JSON that I came up with that works for SolArk inverters:


```
{"destination": "inverter_log",
"columns": [
{"name": "battery_percentage",
"value": "<0/>",
"type": "float"
},
{"name": "load_power",
"value": "<1/>",
"type": "int"
},
{"name": "solar_power",
"value": "<2/> + <3/>",
"type": "int"
}
,
{"name": "battery_voltage",
"value": "<4/>/100",
"type": "float"
},
{"name": "battery_power",
"value": "twosComplement16(<5/>)",
"type": "int"
}
]
}
```
