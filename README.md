# AirAdviser
An affordable, open-source Wi-Fi enabled IoT air pollution detector.

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
