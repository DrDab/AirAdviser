# AirAdviser
<!-- ![alt text](https://raw.githubusercontent.com/DrDab/AirAdviser/master/logo1.png) --!>

AirAdviser

# Building
1. Get the supplies:
  - A NodeMCU + ESP8266 Wi-Fi chip
  - A Plantower PMS5003 Optical Dust sensor
  - A DHT11 temperature/humidity probe
  - Solder (60/40 Rosin core preferred)
  - Jumper cables
  - Hot glue + hot glue gun
  - Epoxy sealant
  - O-ring seal and lubricant
2. Solder the Vin pin on the NodeMCU to the PMS5003's positive contact.
3. Solder the GND pin on the NodeMCU to the PMS5003's ground contact.
4. Solder pin D4 on the NodeMCU to the TX pin on the PMS5003.
5. Solder the 3V3 pin on the NodeMCU to the DHT11's VCC pin and the GND pin on the NodeMCU to the DHT11's GND pin. Solder pin D0 on the NodeMCU to the DATA pin on the DHT11 probe.
6. Power the device on, and flash this code to the NodeMCU. (Remember to download the ESP8266 Arduino plug-ins at https://github.com/esp8266/Arduino/ and DHT11Lib for dht11.h)
7. Encase your survey device into a water-tight, o-ring sealed container. Cut a 4.5-centimeter by 2-centimeter square hole and mount the PMS5003's intake port flush with this hole. Drill a 1cm hole in diameter and mount the DHT11 on it with some tape.
8. Connect to the generated Wi-Fi AP called "AirAdviser-chan XXXX" where XXXX is the last four bytes of the ESP8266's MAC address. The password is "noticemesenpai" by default.
9. To take a reading, open your browser (i.e. Google Chrome) and visit 192.168.4.1/read.

# Usage
AirAdviser is intended to be deployed in low-cost, low-space applications where there are high risks of heatstroke due to low humidity/high temperature and high risks of respiratory harm from air pollution spread accelerated by temperature (such as car exhaust and smoke from chimneys). For example, in cities like Beijing, China this device may have some use. Due to close proximity to the equator, Beijing is known for high temperatures and low relative humidity levels, which contribute to hundreds of thousands of deaths ech year due to conditions such as heatstroke. Adding to that, in lots of areas close to the equator, particulate matter is known to spread faster due to higher temperatures. Global warming has been shown to increase temperatures in equator-locked areas on the globe, which accelerates both phenomenas above. As a result, devices such as AirAdviser allow for quick safety monitoring of living conditions.  

# License
MIT License

Copyright 2018 Victor Du

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
