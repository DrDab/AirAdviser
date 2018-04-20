# AirAdviser
![alt text](https://raw.githubusercontent.com/DrDab/AirAdviser/master/logo1.png)

「AirAdviser」 is the world's first affordable ($50), open-source air pollution and temperature-triggered home automaton controller. It reports levels of particulate matter in the air classified in to categories of particles with a diameter of 1mm, 2.5mm and 10mm. It can also provide a warning if levels become dangerous. This is intended as a form of adaptation to global warming.

# Building
1. Get the supplies:
  - A NodeMCU + ESP8266 Wi-Fi chip
  - A Plantower PMS5003 Optical Dust sensor
  - Solder (60/40 Rosin core preferred)
  - Jumper cables
  - Hot glue + hot glue gun
  - Epoxy sealant
  - O-ring seal and lubricant
2. Solder the Vin pin on the NodeMCU to the PMS5003's positive contact.
3. Solder the GND pin on the NodeMCU to the PMS5003's ground contact.
4. Solder pin D4 on the NodeMCU to the TX pin on the PMS5003.
5. Power the device on, and flash this code to the NodeMCU. (Remember to download the ESP8266 Arduino plug-ins at https://github.com/esp8266/Arduino/)
6. Encase your survey device into a water-tight, o-ring sealed container. Cut a 4.5-centimeter by 2-centimeter square hole and mount the PMS5003's intake port flush with this hole. Seal it with hot glue and epoxy. Seal any gaps in the metal using Flex Tape. 
7. Connect to the generated Wi-Fi AP called "AirAdviser-chan XXXX" where XXXX is the last four bytes of the ESP8266's MAC address. The password is "noticemesenpai" by default.
8. To take a reading, open your browser (i.e. Google Chrome) and visit 192.168.4.1/read.

# Usage
AirAdviser is intended to be deployed in low-cost, low-space applications where non-industrial monitoring of air quality needs to be remotely monitored. For example, AirAdviser can be deployed in high-pollution living areas so tenants can easily and safely check the air pollution levels outside to stay safe and avoid breathing issues. In certain cities, such as Beijing, smog waves, combined with heat waves accelerated by global warming result in frequent storms of airborne particulate matter, which hurts many people. This project can help people adapt to such conditions accelerated by climate change. AirAdviser can detect foreign airborne particulate matter, such as mercury and lead particles, spores, etc. that can cause serious damage to the human body upon exposure. 

Ideas for usage include: 
- Mounting AirAdvisor on a patio wall and attaching it to mains.

# License
MIT License

Copyright 2018 Victor Du

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
