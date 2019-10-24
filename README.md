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

### MQTT status
```
#### Status posted.  0=off 1=on, unless otherwise stated
aquapured/SWG                (0 off, 2 on and generating salt.)
aquapured/SWG/enabled        (0 off, 2 on but not generating salt - SWG reported no-flow or equiv.)
aquapured/SWG/Percent        (SWG Generating %, i.e. 50, value between 0 and 101)
aquapured/SWG/fullstatus     (0 on, 1 no flow, 2 low salt, 4 high salt, 8 clean cell, 9 turning off, 16 high current, 32 low volts, 64 low temp, 128 check PCB, 255 off)
aquapured/SWG/PPM            ( SWG Parts Per Million i.e. 3100)
aquapured/SWG/Boost          ( Boost on or off, 1 or 0)
aquapured/GPIO_13            (GPIO 13 on or off, 1 or 0)
aquapured/GPIO_XX            (XX = any GPIO you've configured)
```
#### Make requests over MQTT
```
Add /set to topic and messages are 0=off, 1=on or number between 1 and 100 for percent. 
aquapured/SWG/set           (Message is 0 or 1, Not really relivent on many systems, will depend on physical Pump to SWG wiring)
aquapured/SWG/Percent/set   (Message is number between 0 and 100)
aquapured/SWG/Boost/set     (Message is 0 or 1)
aquapured/GPIO_13/set       (Message is 0 or 1)
```

