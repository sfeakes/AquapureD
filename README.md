# aquapureD  
linux daemon to control Aquapure SWG without the need for a pool control panel.

## Note
This is still in beta mode.

After downloading
Please edit `./release/aquapured.conf` and change configuration to your setup.

To run
```sudo ./release/aquapured -d -c ./release/aquapured.conf```

### Currently supported
SWG using Jandy protocol (this is most SWG, including Pentair).
MQTT, Web API, Web Sockets and Basic Web UI
Use of GPIO to control relays to turn on/off other pool equiptment.
Homekit integration with homekit-aqualinkd

### Web API
```
http://ip/?command=status                   (return JSON status)
http://ip/?command=swg_percent&value=50     (Set SWG Percent to 50)
http://ip/?command=swg&value=on             (Set SWG on or off)
http://ip/?command=GPIO_13&value=on         (Set GPIO 13 on or off)
```

### MQTT
```
#### Status posted.  0=off 1=on,
aquapured/SWG 0
aquapured/SWG/Percent 30
aquapured/SWG/fullstatus 254
aquapured/SWG/PPM 3100
aquapured/SWG/enabled 0
aquapured/SWG/Boost 0
aquapured/GPIO_13 0
aquapured/GPIO_19 0
aquapured/GPIO_18 0
aquapured/GPIO_16 0
```
#### Make requests over MQTT
```
Add /set to topic and messages are 0=off, 1=on or number between 1 and 100 for percent. 
aquapured/SWG/set
aquapured/SWG/Percent/set
aquapured/SWG/Boost/set
aquapured/GPIO_13/set
```

