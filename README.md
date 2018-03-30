# AirAdviser
An affordable, open-source Wi-Fi enabled IoT air pollution detector. Reports levels of particulate matter in the air classified in to categories of particles with a diameter of 1mm, 2.5mm and 10mm. It can provide a warning if levels become dangerous.

# Building
1. Get the supplies:
  - A NodeMCU + ESP8266 Wi-Fi chip
  - A Plantower PMS5003 Optical Dust sensor
  - Solder (60/40 Rosin core preferred)
  - Jumper cables
2. Solder the VU pin on the NodeMCU to the PMS5003's 5V input.
3. Solder the GND pin on the NodeMCU to the GND output on the PMS5003.
4. Solder the PMS5003's data output (TX) pin to pin D2 on the NodeMCU.
5. Attach a 5.1V 1A power supply to the NodeMCU's micro-USB port.
6. Power the device on, and flash this code to the NodeMCU. (Remember to download the ESP8266 Arduino plug-ins at https://github.com/esp8266/Arduino/)
7. Encase your survey device into a water-tight, o-ring sealed container. Cut a 4.5-centimeter by 2-centimeter square hole and mount the PMS5003's intake port flush with this hole. Seal it with hot glue and epoxy. Seal any gaps in the metal using Flex Tape. 
8. Connect to the generated Wi-Fi AP called "AirAdviser-chan XXXX" where XXXX is the last four bytes of the ESP8266's MAC address. The password is "noticemesenpai" by default.
9. To take a reading, open your browser (i.e. Google Chrome) and visit 192.168.4.1/read.

# License
MIT License

Copyright 2018 Victor Du

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
