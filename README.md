# Dryer Vibration Messenger

A simple non-intrusive way to notify you when your clothes have completed a cycle by posting a message to Twitter.

## Getting Started

You'll need a ESP8266 (I used the Adafruit Huzzah, but any old ESP will do). You just need 2 digital inputs and 2 digital outputs connected to two vibration sensors oriented orthogonal to each other, each pulled up to 3.3V with a 4.7kohm resistor, and two output LEDs for indication. That's it. A small magnetic project box will probably be a wise addition as well. Power at your own discretion :).

### Prerequisites

Under Arduino IDE > Preferences > Additional Boards Manager URLs:
```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Then under Tools:
```
Board: Adafruit HUZZAH ESP8266 (or whatever you are using)
CPU Frequency: 80MHz
Upload Speed: 115200
``` 

### Installing

Libraries you'll need:

```
ESP8266WiFi
```

## Deployment

Edit credentials.h with Wi-Fi SSID, password, IFTTT key, and IFTTT event name. An IFTTT webhook must be setup to consume the data. This repo uses the following Webhooks setup:

```
If Maker Event "dryer_tweeter", then Post a tweet to @DereksDryer
```

```
{{Value1}} Duration: {{Value2}}min, {{Value3}}sec.
```

## Future work
* Add ability to configure unit with Wi-Fi manager on first boot or hard reset.
* Add ability to tune threshold levels in-situ 
* Add reminders to get clothes until door event is triggered

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

## Acknowledgments
https://makezine.com/projects/make-34/the-dryer-messenger/