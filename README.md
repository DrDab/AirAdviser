# AirAdviser
An affordable, open-source Wi-Fi enabled IoT air pollution detector. Reports levels of particulate matter in the air classified in to categories of particles with a diameter of 1mm, 2.5mm and 10mm. It can provide a warning if levels become dangerous.

# Building
1. Get the supplies:
  - A NodeMCU + ESP8266 Wi-Fi chip
  - A 3.3V to 5V Logic Level Shifter
  - A PMS5003 Optical Dust sensor
  - Solder
2. Solder the 3V3 pin on the NodeMCU to the intake on the Logic Level Shifter.
3. Solder the GND pin on the NodeMCU to the GND intake on the Logic Level Shifter.
4. Solder the 5V intake pin on the PMS5003 to the 5V output on the Logic Level Shifter.
5. Solder the GND pin on the PMS5003 to the GND intake on the Logic Level Shifter.
6. Solder the PMS5003's data output (TX) pin to pin D2 on the NodeMCU.
7. Attach a 5.1V 1A power supply to the NodeMCU.
8. Power the device on, and flash this code to the NodeMCU. (Remember to download the ESP8266 Arduino plug-ins at https://github.com/esp8266/Arduino/)
9. Connect to the generated Wi-Fi AP called "AirAdviser-chan XXXX" where XXXX is the last four bytes of the ESP8266's MAC address. The password is "noticemesenpai" by default.
10. To take a reading, open your browser (i.e. Google Chrome) and visit 192.168.4.1/read.

# License
MIT License

Copyright 2018 Victor Du

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
