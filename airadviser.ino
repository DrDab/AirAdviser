/*
 * Copyright (c) 2018 Victor Du
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <dht11.h>

//////////////////////////////////////
// USER CONFIGURABLE SETTINGS BELOW //
//////////////////////////////////////

// The Wi-Fi SSID for the hotspot
const String WIFI_SSID = "AirAdviser-chan ";


// The Wi-Fi password for the hotspot
const char WiFiAPPSK[] = "noticemesenpai";

// Enable or disable serial debugging (will cause webserver slowdown)
const bool USE_SERIAL_DEBUGGING = false;

// Set pins for PMS5003 I/O (Pollution Probe)
const uint8_t PMS_RX = 2;
const uint8_t PMS_TX = 3;

// Set pins for DHT11 I/O (Temperature Probe)
const uint8_t DHT11_PIN = 16;

// Enable or disable warning LED
const bool USE_WARNING_LED = true;
const uint8_t WARNING_LED_PIN = 10; // GPIO10 corresponds to pin SD3 on NodeMCU

// Set port for HTTP webserver
const uint8_t HTTP_SERVER_PORT = 80;

// Set port for appliance control output (D4)
const uint8_t IOT_CONTROL_PORT = 12;

//////////////////////////////////////
// END USER CONFIGURABLE SETTINGS   //
//////////////////////////////////////

WiFiServer server(HTTP_SERVER_PORT);

SoftwareSerial pmsSerial(PMS_RX, PMS_TX);

dht11 DHT11;

/**
 * Probe purchase link:
 * https://www.newegg.com/Product/Product.aspx?Item=9SIAEC96HY0339&ignorebbr=1&nm_mc=KNC-GoogleMKP-PC&cm_mmc=KNC-GoogleMKP-PC-_-pla-_-Gadgets-_-9SIAEC96HY0339&gclid=Cj0KCQjw1-fVBRC3ARIsAIifYOOhSSKaupNA857jX3l2fpRjNlefpp9aKfTHRs_bb6JLetQBgNtP1PcaAi93EALw_wcB&gclsrc=aw.ds
 * MCU + WiFi Chip purchase link:
 * https://www.amazon.com/HiLetgo-Internet-Development-Wireless-Micropython/dp/B010O1G1ES/
 * Solar powered battery pack:
 * https://www.amazon.com/Hiluckey-Waterproof-Shockproof-Travelling-Activities/dp/B075JGP36B/
 * Total cost: $30 excluding case
 */

struct pms5003data
{
  uint16_t framelen;
  uint16_t pm10_standard, pm25_standard, pm100_standard;
  uint16_t pm10_env, pm25_env, pm100_env;
  uint16_t particles_03um, particles_05um, particles_10um, particles_25um, particles_50um, particles_100um;
  uint16_t unused;
  uint16_t checksum;
};
 
struct pms5003data data; 
 
boolean readPMSdata(Stream *s)
{
  if (! s->available())
  {
    return false;
  }
  
  // Read a byte at a time until we get to the special '0x42' start-byte
  if (s->peek() != 0x42)
  {
    s->read();
    return false;
  }
 
  // Now read all 32 bytes
  if (s->available() < 32) 
  {
    return false;
  }
    
  uint8_t buffer[32];    
  uint16_t sum = 0;
  s->readBytes(buffer, 32);
 
  // get checksum ready
  for (uint8_t i=0; i<30; i++)
  {
    sum += buffer[i];
  }
 
  /* debugging
  for (uint8_t i=2; i<32; i++) {
    Serial.print("0x"); Serial.print(buffer[i], HEX); Serial.print(", ");
  }
  Serial.println();
  */
  
  // The data comes in endian'd, this solves it so it works on all platforms
  uint16_t buffer_u16[15];
  for (uint8_t i=0; i<15; i++)
  {
    buffer_u16[i] = buffer[2 + i*2 + 1];
    buffer_u16[i] += (buffer[2 + i*2] << 8);
  }
 
  // put it into a nice struct :)
  memcpy((void *)&data, (void *)buffer_u16, 30);
 
  if (sum != data.checksum)
  {
    Serial.println("Checksum failure");
    return false;
  }
  // success!
  return true;
}


void setup() 
{
  initHardware();
  setupWiFi();
  server.begin();
  pmsSerial.begin(9600);
}

boolean hasReading = false;

// pollution sensor readings.
uint16_t pm10 = 0;
uint16_t pm25 = 0;
uint16_t pm100 = 0;

uint16_t p03 = 0;
uint16_t p05 = 0;
uint16_t p10 = 0;
uint16_t p25 = 0;
uint16_t p50 = 0;
uint16_t p100 = 0;

// dht11 temperature sensor readings.
uint16_t temp_c = 0;
uint16_t humidity = 0;
float heat_index = 0.0;
float dew_point = 0.0;

// deltas cannot be unsigned int because negatives will cause overflow
float pm10_delta = 0.0;
float pm25_delta = 0.0;
float pm100_delta = 0.0;
float temp_delta = 0.0;
float humidity_delta = 0.0;
float heat_index_delta = 0.0;
float dew_point_delta = 0.0;

// values for average calculation
uint32_t p10accum = 0;  float p10avg = 0.0;
uint32_t p25accum = 0;  float p25avg = 0.0;
uint32_t p100accum = 0; float p100avg = 0.0;

uint32_t temp_accum = 0;     float temp_avg = 0.0;
uint32_t humidity_accum = 0; float humidity_avg = 0.0;
float heat_index_accum = 0.0;  float heat_index_avg = 0.0;
float dew_point_accum = 0.0; float dew_point_avg = 0.0;

// array of samples taken each 15 minutes, stored for 7 days.
uint16_t trials_pm10[672];
uint16_t trials_pm25[672];
uint16_t trials_pm100[672];

uint16_t trials_temp[672];
uint16_t trials_humidity[672];
float trials_heat_index[672];
float trials_dew_point[672];

// sample count (take a reading every 15 minutes, and get estimate based on calibration table below
// samples | delta (to 1 minute)
//  900    | +8 seconds
// 1125    | -4 seconds
// 
// 1125 - 900
// ---------- = +18.75 samples to add 1 second
//    12
// 18.75 * 8 = 150 samples added
// 900 + 150 = 1050 samples total
uint16_t samplesperinterval = 1050;
uint16_t numsamples = 0;
uint16_t numremaining = samplesperinterval;

// number of minutes left to next sample.
uint8_t minutes_left = 15;

// number of trials (increase by 1 each time we get 1050 readings. also the position to write to in the array.
uint16_t ptrial = 0;

boolean writeSerial = true;

// Cloud Fox encoded image
const char* cloud_fox = "data:image/png;base64,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";

float tofahrenheit(float temp)
{
  return (1.8 * temp) + 32.0;
}

// auxiliary function to calculate the heatstroke index based on RH% and temperature (Fahrenheit).
float heatstroke_index(float temp_f, float rh)
{
  float hi_f = -42.379 + 2.04901523*temp_f + 10.14333127*rh - .22475541*temp_f*rh - .00683783*temp_f*temp_f - .05481717*rh*rh + .00122874*temp_f*temp_f*rh + .00085282*temp_f*rh*rh - .00000199*temp_f*temp_f*rh*rh;
  return (hi_f - 32.0) * 0.55555556;
}

// auxiliary function to calculate the dew point based on RH% and temperature (Celsius)
float dew_point_get(float temp_c, float rh)
{
  float a = 17.271;
  float b = 237.7;
  float temp = (a * temp_c) / (b + temp_c) + log(rh*0.01);
  float Td = (b * temp) / (a - temp);
  return Td;
}

void loop() 
{
  runServer();
}

int chk;
bool dhtREAD = false;

void runServer()
{
  dhtREAD = true;
  bool haveCurrentReading = true;
  if (readPMSdata(&pmsSerial)) 
  {
    // increment the number of samples by one.
    numsamples++;
    
    hasReading = true;
    
    // calculate the delta values.
    pm10_delta = (float)data.pm10_standard - p10avg;
    pm25_delta = (float)data.pm25_standard - p25avg;
    pm100_delta = (float)data.pm100_standard - p100avg;

    // calculate the temperature.
    chk = DHT11.read(DHT11_PIN);
    switch (chk)
    {
      case DHTLIB_OK: 
        Serial.println("DHT11: OK"); 
        break;
      case DHTLIB_ERROR_CHECKSUM: 
        Serial.println("DHT11: Checksum error"); 
        dhtREAD = false;
        break;
      case DHTLIB_ERROR_TIMEOUT: 
        Serial.println("DHT11: Time out error"); 
        dhtREAD = false;
        break;
      default: 
        Serial.println("DHT11: Unknown error"); 
        dhtREAD = false;
        break;
    }

    if (dhtREAD)
    {
      // read the value from the DHT11.
      temp_c = DHT11.temperature;
      temp_delta = (float) temp_c - temp_avg;
      humidity = DHT11.humidity;
      humidity_delta = (float) humidity - humidity_avg;
    }

    // calculate the heat index.
    heat_index = heatstroke_index(tofahrenheit(temp_c), humidity);
    heat_index_delta = heat_index - heat_index_avg;

    // calculate the dew point.
    dew_point = dew_point_get(temp_c, humidity);
    dew_point_delta = dew_point - dew_point_avg;

    if (ptrial == 672)
    {
      // reset every 672 trials to prevent integer overflow and array fillup.
      numsamples = 0;
      p10accum = 0;
      p25accum = 0;
      p100accum = 0;
      ptrial = 0;
      p10avg = 0.0;
      p25avg = 0.0;
      p100avg = 0.0;
      temp_accum = 0;
      temp_avg = 0.0;
      humidity_accum = 0;
      humidity_avg = 0.0;
      heat_index_accum = 0.0;
      heat_index_avg = 0.0;
      dew_point_accum = 0.0;
      dew_point_avg = 0.0;
      // clear the array of trials.
      memset(trials_pm10, 0, sizeof(trials_pm10));
      memset(trials_pm25, 0, sizeof(trials_pm25));
      memset(trials_pm100, 0, sizeof(trials_pm100));
      memset(trials_temp, 0, sizeof(trials_temp));
      memset(trials_humidity, 0, sizeof(trials_humidity));
      memset(trials_heat_index, 0, sizeof(trials_heat_index));
      memset(trials_dew_point, 0, sizeof(trials_dew_point));
    }

    // calculate the time to the next sample.
    uint16_t numremaining = samplesperinterval - numsamples;
    minutes_left = (uint8_t)(((float)numremaining / (float)samplesperinterval) * 15.0);
    
    // calculate the averages.
    if (numsamples % samplesperinterval == 0)
    {
      numsamples = 0;
      numremaining = samplesperinterval;
      trials_pm10[ptrial] = data.pm10_standard;
      trials_pm25[ptrial] = data.pm25_standard;
      trials_pm100[ptrial] = data.pm100_standard;
      trials_temp[ptrial] = temp_c;
      trials_humidity[ptrial] = humidity;
      trials_heat_index[ptrial] = heat_index;
      trials_dew_point[ptrial] = dew_point_delta;
      p10accum += data.pm10_standard;
      p25accum += data.pm25_standard;
      p100accum += data.pm100_standard;
      temp_accum += temp_c;
      humidity_accum += humidity;
      heat_index_accum += heat_index;
      dew_point_accum += dew_point;
      ptrial++;
      p10avg = (float)p10accum / (float)ptrial;
      p25avg = (float)p25accum / (float)ptrial;
      p100avg = (float)p100accum / (float)ptrial; 
      temp_avg = temp_accum / (float)ptrial;
      humidity_avg = humidity_accum / (float)ptrial; 
      heat_index_avg = heat_index_accum / (float)ptrial;
      dew_point_avg = dew_point_accum / (float)ptrial;
    }
    
    pm10 = data.pm10_standard;
    pm25 = data.pm25_standard;
    pm100 = data.pm100_standard;
    p03 = data.particles_03um;
    p05 = data.particles_05um;
    p10 = data.particles_10um;
    p25 = data.particles_25um;
    p50 = data.particles_50um;
    p100 = data.particles_100um;
    
    if (USE_SERIAL_DEBUGGING)
    {
        Serial.println();
        if (data.pm25_env >= 500)
        {
          Serial.println("\nDANGER\n");
          Serial.println("Air pollution levels are unsafe! Do not go outdoors\n");
        }
        Serial.println("---------------------------------------");
        Serial.println("Temperature Readings");
        Serial.print("Degrees (C) "); Serial.print(temp_c);
        Serial.print("\t\tDelta (C): "); Serial.print(temp_delta);
        Serial.print("\t\tAverage (C): "); Serial.print(temp_avg);
        Serial.println();
        Serial.println("Humidity Readings");
        Serial.print("%RH "); Serial.print(humidity);
        Serial.print("\t\tDelta: "); Serial.print(humidity_delta);
        Serial.print("\t\tAverage: "); Serial.print(humidity_avg);
        Serial.println("Calculated Info");
        Serial.print("Heat Index: "); Serial.print(heat_index);
         Serial.print("\t\tDew Point (C): "); Serial.print(dew_point);
        Serial.println();
        Serial.println("---------------------------------------");
        Serial.println("Concentration Units (standard)");
        Serial.print("PM 1.0: "); Serial.print(data.pm10_standard);
        Serial.print("\t\tPM 2.5: "); Serial.print(data.pm25_standard);
        Serial.print("\t\tPM 10: "); Serial.print(data.pm100_standard);
        Serial.print("\nPM 1.0 Delta: "); Serial.print(pm10_delta);
        Serial.print("\t\tPM 2.5 Delta: "); Serial.print(pm25_delta);
        Serial.print("\t\tPM 10 Delta: "); Serial.println(pm100_delta);
        Serial.println("---------------------------------------");
        Serial.println("Concentration Units (environmental)");
        Serial.print("PM 1.0: "); Serial.print(data.pm10_env);
        Serial.print("\t\tPM 2.5: "); Serial.print(data.pm25_env);
        Serial.print("\t\tPM 10: "); Serial.println(data.pm100_env);
        Serial.println("---------------------------------------");
	      Serial.println("Averages");
        Serial.print("PM 1.0 avg: "); Serial.print(p10avg);
        Serial.print("\t\tPM 2.5 avg: "); Serial.print(p25avg);
        Serial.print("\t\tPM 10 avg: "); Serial.print(p100avg);
	      Serial.print("\t\tN= "); Serial.println(ptrial);
        Serial.println("---------------------------------------");
        Serial.print("Particles > 0.3um / 0.1L air:"); Serial.println(data.particles_03um);
        Serial.print("Particles > 0.5um / 0.1L air:"); Serial.println(data.particles_05um);
        Serial.print("Particles > 1.0um / 0.1L air:"); Serial.println(data.particles_10um);
        Serial.print("Particles > 2.5um / 0.1L air:"); Serial.println(data.particles_25um);
        Serial.print("Particles > 5.0um / 0.1L air:"); Serial.println(data.particles_50um);
        Serial.print("Particles > 50 um / 0.1L air:"); Serial.println(data.particles_100um);
        Serial.println("---------------------------------------");
    }
    if (data.pm25_standard >= 55)
    {
      digitalWrite(WARNING_LED_PIN, HIGH);
    }
    else
    {
      digitalWrite(WARNING_LED_PIN, LOW);
    }
  }
  else
  {
    haveCurrentReading = false;
  }
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client)
  {
    return;
  }
  writeSerial = false;
  Serial.println("Ready");
  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.println(req);
  client.flush();


  int val = -1; 
  
  if (req.indexOf("/about") != -1)
  {
    val = 0; // about page
  }
  else if (req.indexOf("/read") != -1)
  {
    val = -2; // read page
  }
  else if (req.indexOf("/results.csv") != -1)
  {
    val = -5;
  }
  else if (req.indexOf("/manage") != -1)
  {
    val = -8;
  }
  else if (req.indexOf("/control.red") != -1)
  {
    val = -9;
  }

  String tmpStr = "";
  if (val == -5)
  {
    // read the array
    tmpStr += "Sample #, Temperature, %RH, Heat Index, Dew Point, PM1.0, PM2.5, PM10.0\n";
    for(uint16_t i = 0; i < ptrial; i++)
    {
      tmpStr += i;
      tmpStr += ","; 
      tmpStr += trials_temp[i];
      tmpStr += ",";
      tmpStr += trials_humidity[i];
      tmpStr += ",";
      tmpStr += trials_heat_index[i];
      tmpStr += ",";
      tmpStr += trials_dew_point[i];
      tmpStr += ",";
      tmpStr += trials_pm10[i];
      tmpStr += ",";
      tmpStr += trials_pm25[i];
      tmpStr += ",";
      tmpStr += trials_pm100[i];
      tmpStr += "\n";
    }
    tmpStr += "\nEND OF LOG";
    Serial.println("CSV Log requested.");
    client.print(tmpStr);
    client.flush();
    delay(1);
    return;
  }


  // Prepare the response. Start with the common header:
  String o = "";
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n\r\n";
  s += "<!DOCTYPE HTML>\r\n<html>\r\n";
  s += "<head>";
  s += "<style>";
  s += "body {";
  s += "  background-color: black;";
  s += "  color: lime;";
  s += "  font-family:monospace;";
  s += "}";
  s += "</style>";
  s += "</head>";
  if (val >= 0)
  {
    Serial.println("About page requested by client.");
    s += "<title>About</title>";
    s += "<strong>About</strong><br> ";
    s += "AirAdviser Beta version 0.2-floof-fox by Victor, Leodis and Carter";
    s += "<br>";
    s += "<a href=\"192.168.4.1/read\">Read sensor data</a>";
    s += "<br><p>Cloud Fox</p>";
    s += "<img src=\"";
    s += cloud_fox;
    s += "\">";
    //client.print(s);
    //client.print(cloud_fox);
    //client.print(o);
    //client.flush();
    //delay(1);
    //Serial.println("Client disconnected");
    //return;
  }
  else if (val == -2)
  { 
    Serial.println("Reading page requested by client.");
    s += "<meta http-equiv=\"refresh\" content=\"2\" >";
    s += "<title>Sensor Reading</title>";
    s += "<strong>Sensor Data</strong>";

    s += "<br>";
    s += "========================================";
    s += "<br>";
    s += "Weather Information";
    s += "<br>";
    s += "Current Temperature: ";
    s += String(temp_c);
    s += " &deg;C";
    s += " (&Delta;(&mu;,c)= ";
    s += String(temp_delta);
    s += " &mu;= ";
    s += String(temp_avg);
    s += " N= ";
    s += String(ptrial);
    s += ")";
    s += "<br>";
    s += "% Relative Humidity: ";
    s += String(humidity);
    s += "%";
    s += " (&Delta;(&mu;,c)= ";
    s += String(humidity_delta);
    s += " &mu;= ";
    s += String(humidity_avg);
    s += " N= ";
    s += String(ptrial);
    s += ")";
    s += "<br>";
    s += "Heat Index: ";
    s += String(heat_index);
    s += " &deg;C";
    s += " (&Delta;(&mu;,c)= ";
    s += String(heat_index_delta);
    s += " &mu;= ";
    s += String(heat_index_avg);
    s += " N= ";
    s += String(ptrial);
    s += ")";
    s += "<br>";
    s += "Dew Point: ";
    s += String(dew_point);
    s += " &deg;C (&Delta;(&mu;,c)= ";
    s += String(dew_point_delta);
    s += " &mu;= ";
    s += String(dew_point_avg);
    s += " N= ";
    s += String(ptrial);
    s += ")";
    s += "<br>";
    s += "========================================";
    s += "<br>";
    s += "Air Quality Information";
    s += "<br>";
    if (haveCurrentReading) 
    {
      hasReading = true;
      Serial.println("Data collection successful, sending to client.");
      s += "PM1.0 Level: ";
      s += String(pm10);
      s += " &mu;g/m^3";
      s += " (&Delta;(&mu;,c)= ";
      s += String(pm10_delta);
      s += " &mu;= ";
      s += String(p10avg);
      s += " N= ";
      s += String(ptrial);
      s += ")";
      s += "<br>";
      s += "PM2.5 Level: ";
      s += String(pm25);
      s += " &mu;g/m^3";
      s += " (&Delta;(&mu;,c)= ";
      s += String(pm25_delta);
      s += " &mu;= ";
      s += String(p25avg);
      s += " N= ";
      s += String(ptrial);
      s += ")";
      s += "<br>";
      s += "PM10.0 Level: ";
      s += String(pm100);
      s += " &mu;g/m^3";
      s += " (&Delta;(&mu;,c)= ";
      s += String(pm100_delta);
      s += " &mu;= ";
      s += String(p100avg);
      s += " N= ";
      s += String(ptrial);
      s += ")";
      s += "<br>";
      s += "========================================";
      s += "<br>";
      s += "Particles > 0.3 &mu;m / 0.1L air: ";
      s += String(p03);
      s += "<br>";
      s += "Particles > 0.5 &mu;m / 0.1L air: ";
      s += String(p05);
      s += "<br>";
      s += "Particles > 1.0 &mu;m / 0.1L air: ";
      s += String(p10);
      s += "<br>";
      s += "Particles > 2.5 &mu;m / 0.1L air: ";
      s += String(p25);
      s += "<br>";
      s += "Particles > 5.0 &mu;m / 0.1L air: ";
      s += String(p50);
      s += "<br>";
      s += "Particles > 50 &mu;m / 0.1L air : ";
      s += String(p100);
      s += "<br>";
      // begin warnings
      
      s += "<br><br>";
      s += "<table style=\"background-color: #000; border: 2px solid #00ff00; padding: 1px;\" cellpadding=\"0\" cellspacing=\"0\" align=\"center\">";
      s += "<tr>";
      s += "<td style=\"padding:2px\">";
      s += "<center>";
      s += "<big>";
      s += "<b>Pollution Advisory</b>";
      s += "</big>";
      s += "</center>";
      s += "<br //>";
      s += "<center>";
      if (pm25 >= 500)
        {
          s += "<strong>&#9760;DANGER: POLLUTION LEVELS HAZARDOUS!</strong>";
          s += "<br>";
          s += "PM2.5 Levels are over 500.0 &mu;g/m^3";
          s += "<br>";
          s += "Do NOT go outdoors. If going outdoors, YOU risk:";
          s += "<br>";
          s += "<strong>- Serious aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly</strong>";
          s += "<br>";
          s += "<strong>- Serious risk of respiratory effects in general population. </strong>";
        } 
        else if (pm25 < 500 && pm25 >= 250)
        {
          s += "<strong>&#9763;CAUTION: POLLUTION LEVELS VERY UNHEALTHY</strong>";
          s += "<br>";
          s += "PM2.5 Levels are over 250.0 &mu;g/m^3";
          s += "<br>";
          s += "Do NOT go outdoors. If going outdoors, YOU risk:";
          s += "<br>";
          s += "<strong>- Significant aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly</strong>";
          s += "<br>";
          s += "<strong>- Significant increase in respiratory effects in general population. </strong>";
        }
        else if (pm25 < 250 && pm25 >= 150)
        {
          s += "<strong>&#9888;CAUTION: POLLUTION LEVELS UNHEALTHY</strong>";
          s += "<br>";
          s += "PM2.5 Levels are over 150.0 &mu;g/m^3";
          s += "<br>";
          s += "AVOID prolonged exertion outdoors. YOU risk:";
          s += "<br>";
          s += "<strong>- Increased aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly</strong>";
          s += "<br>";
          s += "<strong>- Increased respiratory effects in general population.</strong>";
        }
        else if (pm25 < 150 && pm25 >= 55)
        {
          s += "<strong>&#9888;CAUTION: POLLUTION LEVELS UNHEALTHY FOR SENSITIVE PEOPLE</strong>";
          s += "<br>";
          s += "PM2.5 Levels are over 55.0 &mu;g/m^3";
          s += "<br>";
          s += "AVOID prolonged exertion outdoors. YOU risk:";
          s += "<br>";
          s += "<strong>- Increased aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly</strong>";
          s += "<br>";
          s += "<strong>- Increased respiratory effects in general population.</strong>";
        }
        else
        {
          s += "<strong>&#128175;&#128293;POLLUTION LEVELS ARE HEALTHY&#128175;&#128293;</strong>";
        }
        s += "</center>";
        s += "</td></tr></table>";

        // begin heatstroke warnings
        s += "<br><br>";
        s += "<table style=\"background-color: #000; border: 2px solid #00ff00; padding: 1px;\" cellpadding=\"0\" cellspacing=\"0\" align=\"center\">";
        s += "<tr>";
        s += "<td style=\"padding:2px\">";
        s += "<center>";
        s += "<big>";
        s += "<b>Heatstroke Advisory</b>";
        s += "</big>";
        s += "</center>";
        s += "<br //>";
        s += "<center>";
        if(heat_index >= 52.2222)
        {
          // extreme danger
          s += "<strong>&#9888;SEVERE DANGER</strong>";
          s += "<br>";
          s += "Heatstroke or sunstroke likely.";
          s += "<br>";
          s += "Seek shelter <strong>IMMEDIATELY.</strong> Take sips of cold water, and turn on air conditioning. If indoors, <strong>REMAIN INDOORS.</strong>";
        }
        else if (heat_index < 52.2222 && heat_index >= 40)
        {
          // danger
          s += "<strong>&#9888;DANGER</strong>";
          s += "<br>";
          s += "Heat exhaustion, sunstroke and muscle cramps likely.";
          s += "<br>";
          s += "Seek shelter as soon as possible. Take sips of cold water, and turn on air conditioning.";
        }
        else if (heat_index < 40.0 && heat_index >= 32.7778)
        {
          // extreme caution
          s += "<strong>&#9888;EXTREME CAUTION</strong>";
          s += "<br>";
          s += "Sunstroke, muscle cramps and heat exhaustion possible.";
          s += "<br>";
          s += "Seek shelter within 2-3 hours. Take sips of cold water, and turn on air conditioning.";
        }
        else if (heat_index < 32.7778 && heat_index >= 26.6667)
        {
          // caution
          s += "<strong>&#9888;CAUTION</strong>";
          s += "<br>";
          s += "Fatigue possible.";
          s += "<br>";
        }
        else
        {
          s += "<strong>&#128175;&#128293;HEAT LEVELS ARE HEALTHY&#128175;&#128293;</strong>";
          // no warnings
        }
        
        s += "</center>";
        s += "</td></tr></table>";
    }
    else
    {
      if (!hasReading)
      {
        s += "No data yet";
        s += "<br>";
      }
      else
      {
        s += "PM1.0 Level: ";
      	s += String(pm10);
      	s += " &mu;g/m^3";
      	s += " (&Delta;(&mu;,c)= ";
      	s += String(pm10_delta);
     	  s += " &mu;= ";
      	s += String(p10avg);
      	s += " N= ";
      	s += String(ptrial);
      	s += ")";
      	s += "<br>";
      	s += "PM2.5 Level: ";
      	s += String(pm25);
      	s += " &mu;g/m^3";
      	s += " (&Delta;(&mu;,c)= ";
      	s += String(pm25_delta);
      	s += " &mu;= ";
      	s += String(p25avg);
      	s += " N= ";
      	s += String(ptrial);
      	s += ")";
      	s += "<br>";
      	s += "PM10.0 Level: ";
      	s += String(pm100);
      	s += " &mu;g/m^3";
      	s += " (&Delta;(&mu;,c)= ";
      	s += String(pm100_delta);
      	s += " &mu;= ";
      	s += String(p100avg);
      	s += " N= ";
      	s += String(ptrial);
      	s += ")";
      	s += "<br>";
        s += "========================================";
        s += "<br>";
        s += "Particles > 0.3 &mu;m / 0.1L air: ";
        s += String(p03);
        s += "<br>";
        s += "Particles > 0.5 &mu;m / 0.1L air: ";
        s += String(p05);
        s += "<br>";
        s += "Particles > 1.0 &mu;m / 0.1L air: ";
        s += String(p10);
        s += "<br>";
        s += "Particles > 2.5 &mu;m / 0.1L air: ";
        s += String(p25);
        s += "<br>";
        s += "Particles > 5.0 &mu;m / 0.1L air: ";
        s += String(p50);
        s += "<br>";
        s += "Particles > 50 &mu;m / 0.1L air : ";
        s += String(p100);
        s += "<br>";

        // begin pollution warnings
        s += "<br><br>";
        s += "<table style=\"background-color: #000; border: 2px solid #00ff00; padding: 1px;\" cellpadding=\"0\" cellspacing=\"0\" align=\"center\">";
        s += "<tr>";
        s += "<td style=\"padding:2px\">";
        s += "<center>";
        s += "<big>";
        s += "<b>Pollution Advisory</b>";
        s += "</big>";
        s += "</center>";
        s += "<br //>";
        s += "<center>";
        if (pm25 >= 500)
        {
          s += "<strong>&#9760;DANGER: POLLUTION LEVELS HAZARDOUS!</strong>";
          s += "<br>";
          s += "PM2.5 Levels are over 500.0 &mu;g/m^3";
          s += "<br>";
          s += "Do NOT go outdoors. If going outdoors, YOU risk:";
          s += "<br>";
          s += "<strong>- Serious aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly</strong>";
          s += "<br>";
          s += "<strong>- Serious risk of respiratory effects in general population. </strong>";
        } 
        else if (pm25 < 500 && pm25 >= 250)
        {
          s += "<strong>&#9763;CAUTION: POLLUTION LEVELS VERY UNHEALTHY</strong>";
          s += "<br>";
          s += "PM2.5 Levels are over 250.0 &mu;g/m^3";
          s += "<br>";
          s += "Do NOT go outdoors. If going outdoors, YOU risk:";
          s += "<br>";
          s += "<strong>- Significant aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly</strong>";
          s += "<br>";
          s += "<strong>- Significant increase in respiratory effects in general population. </strong>";
        }
        else if (pm25 < 250 && pm25 >= 150)
        {
          s += "<strong>&#9888;CAUTION: POLLUTION LEVELS UNHEALTHY</strong>";
          s += "<br>";
          s += "PM2.5 Levels are over 150.0 &mu;g/m^3";
          s += "<br>";
          s += "AVOID prolonged exertion outdoors. YOU risk:";
          s += "<br>";
          s += "<strong>- Increased aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly</strong>";
          s += "<br>";
          s += "<strong>- Increased respiratory effects in general population.</strong>";
        }
        else if (pm25 < 150 && pm25 >= 55)
        {
          s += "<strong>&#9888;CAUTION: POLLUTION LEVELS UNHEALTHY FOR SENSITIVE PEOPLE</strong>";
          s += "<br>";
          s += "PM2.5 Levels are over 55.0 &mu;g/m^3";
          s += "<br>";
          s += "AVOID prolonged exertion outdoors. YOU risk:";
          s += "<br>";
          s += "<strong>- Increased aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly</strong>";
          s += "<br>";
          s += "<strong>- Increased respiratory effects in general population.</strong>";
        }
        else
        {
          s += "<strong>&#128175;&#128293;POLLUTION LEVELS ARE HEALTHY&#128175;&#128293;</strong>";
        }
        s += "</center>";
        s += "</td></tr></table>";

        // begin heatstroke warnings
        s += "<br><br>";
        s += "<table style=\"background-color: #000; border: 2px solid #00ff00; padding: 1px;\" cellpadding=\"0\" cellspacing=\"0\" align=\"center\">";
        s += "<tr>";
        s += "<td style=\"padding:2px\">";
        s += "<center>";
        s += "<big>";
        s += "<b>Heatstroke Advisory</b>";
        s += "</big>";
        s += "</center>";
        s += "<br //>";
        s += "<center>";
        if(heat_index >= 52.2222)
        {
          // extreme danger
          s += "<strong>&#9888;SEVERE DANGER</strong>";
          s += "<br>";
          s += "Heatstroke or sunstroke likely.";
          s += "<br>";
          s += "Seek shelter <strong>IMMEDIATELY.</strong> Take sips of cold water, and turn on air conditioning. If indoors, <strong>REMAIN INDOORS.</strong>";
        }
        else if (heat_index < 52.2222 && heat_index >= 40)
        {
          // danger
          s += "<strong>&#9888;DANGER</strong>";
          s += "<br>";
          s += "Heat exhaustion, sunstroke and muscle cramps likely.";
          s += "<br>";
          s += "Seek shelter as soon as possible. Take sips of cold water, and turn on air conditioning.";
        }
        else if (heat_index < 40.0 && heat_index >= 32.7778)
        {
          // extreme caution
          s += "<strong>&#9888;EXTREME CAUTION</strong>";
          s += "<br>";
          s += "Sunstroke, muscle cramps and heat exhaustion possible.";
          s += "<br>";
          s += "Seek shelter within 2-3 hours. Take sips of cold water, and turn on air conditioning.";
        }
        else if (heat_index < 32.7778 && heat_index >= 26.6667)
        {
          // caution
          s += "<strong>&#9888;CAUTION</strong>";
          s += "<br>";
          s += "Fatigue possible.";
          s += "<br>";
        }
        else
        {
          s += "<strong>&#128175;&#128293;HEAT LEVELS ARE HEALTHY&#128175;&#128293;</strong>";
          // no warnings
        }
        
        s += "</center>";
        s += "</td></tr></table>";
        
      }
    }
    s += "<br>";
    s += "Estimated time to next log entry:<br>";
    s += String(minutes_left);
    s += " minutes (";
    s += numsamples;
    s += " samples)";
    s += "<br><br>This page last updated at:";
    s += "<p id=\"time\"></p>";
    s += "<script>";
    s += "document.getElementById(\"time\").innerHTML = Date();";
    s += "</script>";
    s += "<a href=\"192.168.4.1/about\">About AirAdvisor</a>";
  }
  else if (val == -8)
  {
    // offer a management page.
    Serial.println("Management page requested by client.");
    s += "<title>Management</title>";
    s += "<strong>AirAdviser Manager</strong><br>";
    s += "OUTPUT ";
    s += String(IOT_CONTROL_PORT);
    s += "<br>Turn ON if:<br>";
    s += "<form action=\"/control.red\">";
    s += "Temp >=:<br>";
    s += "<input type=\"text\" name=\"tmp\"><br>";
    s += "RH >=: <br>";
    s += "<input type=\"text\" name=\"rh\"><br>";
    s += "HI >=: <br>";
    s += "<input type=\"text\" name=\"hi\"><br>";
    s += "DP >=: <br>";
    s += "<input type=\"text\" name=\"dp\"><br>";
    s += "PM 2.5 >=: <br>";
    s += "<input type=\"text\" name=\"pm\"><br>";
    s += "<select name=\"logic\">";
    s += "<option value=\"1\">AND</option>";
    s += "<option value=\"0\">OR</option>";
    s += "</select>";
    s += "<input type=\"submit\" value=\"Set Triggers\">";
    s += "</form>";
  }
  else if (val == -9)
  {
    // parse the management page information.
    
  }
  else
  {
    Serial.println("404 page requested by client.");
    s += "  Hi!<br>";
    s += "I'm Cloud, your personal safeguard for<br>";
    s += "air safety. With my instincts, I can<br>";
    s += "sniff out invisible dangers in the air<br>";
    s += "and keep you safe.<br>";
    s += "\\<br>&nbsp;\\<br>&nbsp;&nbsp;\\<br>";
    s += "<img src=\"";
    s += cloud_fox;
    s += "\">";
    s += "ERROR 404 : Page Not Found.";
    s += "<br>";
    s += "<br>";
    s += "<a href=\"192.168.4.1/read\">Read sensor data</a>";
  }
  s += "</html>\n";

  // Send the response to the client
  client.print(s);
  delay(3);
  Serial.println("Client disconnected");

  // The client will actually be disconnected 
  // when the function returns and 'client' object is detroyed
}

void setupWiFi()
{
  WiFi.mode(WIFI_AP);

  // Do a little work to get a unique-ish name. Append the
  // last two bytes of the MAC (HEX'd) to "Thing-":
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  String AP_NameString = WIFI_SSID + macID;

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i=0; i<AP_NameString.length(); i++)
  {
    AP_NameChar[i] = AP_NameString.charAt(i);
  }

  WiFi.softAP(AP_NameChar, WiFiAPPSK);
}

void initHardware()
{
  Serial.begin(115200);
  Serial.println("Serial monitor is online, running at 115200 baud");
  if (USE_WARNING_LED)
  {
    pinMode(WARNING_LED_PIN, OUTPUT);
    Serial.print("Warning LED initialized on pin="); 
    Serial.println(WARNING_LED_PIN);
    digitalWrite(WARNING_LED_PIN, LOW); 
  }
}

//,*(\*;""";/1Ie1*- -  `<r>,+i;^VMy\?tGb<>_>_>++}tYT1YyoJ#VFnaeFjV4FV3JJ2VJ4eanouF4Itv\=<(uXL:^  - 
//'>>>;"||YZei,` .. ."vr! `.'.v3i>,,_~>);;?><>ijc"v]CVI1}YztoIV#3aeeo4J33n3I24a#VeCaIVIY?;;"Ya?  --
//',<><>{kz_'''.`-.+vr.`.` `>Ti'.^__~<_+<<r72$["i{LxLl1kX1]111yfnIk3II4a4ee3#eIV#VVe34V3I2{/r=aI+'.
//-^>+(kI^`' ' -`:<> ``-'..iY:.`   =s"\|viiy[r;/vii{{Y[11$ZC1fFIe4ZIzzzJ4VX4VV#IkV$3Xe5X#a537==iZY`
//'~!t5=_: '' -.- .`--..`!t( `''--!J!--^_,<>,+=r|?ixL{YY1Y136%#JfTzsztsff1fIZ#k5aIe3eIIa$Z3ZZV7r;nZ
// ,eF<<+<~:?T2^. -- .`.!Y!` '-` -v)`-.'-..:_^,:~<>*|i{]x]1[Tk3TY1zTfTYY1Tx|xhIkV3I44$k55Z5$#Zq4irv
//~e1;?iYVI9NDi^~~!: .`+i!-. .'  '2>`-.- -.--'.' ~_^:,><*r*==r[3{vvv/\|*"<!_->o0ZXZk555k55XkPXZX6ur
//^**t5ay\;umv;*==)\r<"v<!_<>_!~~+#_!~_:~!!~!~` .  .'' ,,_,:,~:+Fv!,^~  '  . `.iS$kXZk$kXkhPkIXEwKI
//!yZT">"*1Dv;/i}}7]TYnclvvxl]v)="e||/;*;;=**++,^^:!,!.`. ``   .:7T~^.`.~.,~^'- "XVZkk$Zk5bU9Vzi>,.
// <<><><rOY|vLY{YYY1f1yssfyjtttzu9sYTY1YYT[{[vi(*;"r<+>>>=<>>><+<1f";)/?;|vv\(/=iw#X00pFTi< `'.`'~
// `^__,+jm)i[TYT1Yz$K1sttzsfy22aV9aFIn3oIuJnojstsFztyy1yszTs111ztt3pJ2tC2CFVnaV22tdVi^`.. '-_^~,rY
//-` ''!>01|iT1TTyh&g6YsftyyyysyCuSCja#IC3V4#eeI3II#e3V3$#3Vn43e4ene5O4X4Z5aa$k#$XVX3<:   ..-!*{Cnt
// -  ` <8v/1}T1eM$7qVTsytjftstJCJ43CuCJ3CeVeCe3nnae4e4e4e34aV#34a43IAMZ$Z4Z5$5X5$eZpz\^~.^>laP4ys1
// .'.  >p?vTTY0$v)ra611zst2jjzuoe$5uCFJF2n222uuCjuJC#VI3e5AhApS65kk#5OK#ZZ5a$kZXk$EIUTi<=fX$aCt2}>
//' `-`.>%=iTebc==r|xOt1ytfo2oC4mOMDau2auoJoFaFonC#njnV5qDMMOOMMOGOp5eINEVV33#X55$5Ve#X1s5kCoFC}>.-
// `-.' ~Iv}ZI<<<+;\;OPssystsznKau%EMaF2onaJJ4uJnV09mw8Ow3#34V#4$A0ODO6%OeZ5$Vaea3V435U#%J2jjf",  -
// `` - .Ls9Y~!<++*=Y@ZtytyffJfyjVn125aan34V3VJoI2OFaJFo3V5X4#IFjn43wb@D&%53$X#eV3#VeahOzuJJ}+^~:'.
//-.'-.'`'Y|   ``,<tK*"KsytfyusF26tZzYu5n3IIJI3VVJbJJJuFJszsJd99VouoJIeP@MG95a3##a$e3e3Mjjj],-  - '
//  ..-  .  '  '-"wn^(1"3ttytIVnoO@B&l-)#V2o2e2##2ZZuYr+;[ZNHg&RRO$aou2nVOIXd5#I#e##IIeMnos,-`-'. -
//.` -'`-' `  .+J6(+>`'`:1jjf$py#WRSR&+'!cIFFC3unoVP1]:'_CgDXoo8WRBN%uVe40sVT6GIeIe4##eMfY"'`- ''..
// . -`. '`-_|JZ1~-`'` ` !CPF#@tbD5OgQT'- _vffJFjuuE1i!~kHNfzUgDDBBQHqekCwyuz>yq3aaea44Mr,-..`.-' :
//~_  '  ':nEy;.  .- `:=yoT*iJ&swoZR&OD! ' -<v[fFzJej?^eQNy1KgW&N&&NQQ@oj3YYT+_vMe#e$IZG:` . .`,_>?
//` ` ''-iMy-`  --'-^in1*<vYyC&[|~ivXKC)`~'!,-^r]Yz1ov*%O]-LbSOU@Pr$@@HNz?1T{~''<34eIoEC `''-`'^<vC
//  ':,<4S=   '.-_|2yv<~c3v^`|M  'TzLS/=-,_!,   ~+"iii_`!  ^,'"SWY TQ8DNz/>?*, ` :C3aeG+-'- -` ~ryD
//' _+c$7-' - </{]/~"<++Z; '!%z' .jDTB&} ,`'.   ``  ~!,.`. xYv($RL-kQhARj+ .: `'`.!#uZv.-'` . ,:+kO
//`,)Zs~`-'  .```+izi"*<m<.^J%_`.-eHym@].~``.--'` `..`..`'.VgEjOH@0DB4qQK:```' .-` >Cs--   '-  !+Y&
//^3n<`  . '.-`:Y3i"r"")&(iTT~`_\~5HKy8"`_.' '`. .`   ''- -#HKVDmAB9XzN1^  ''.`''.'',.- .` -~!<iaOk
//^<.`'.`-`'' ,iE*;*){zIEt\` ``_%ODW@Dt!-_.. '-'  ''-`'``^+qRNbDfwNDab9: `   -- '.'.-:;lz1]YfjGPT=<
//' -`''`'- `-+eY?=r$I=-`'` .^<++;;iT1!  .!_ . .. .  '..|M0QWH@DNQD@DQC .``.-   ``'. _)@Xn#eVYr>+>+
//'  '.` '' :"Lj;"")(Iy'`.`<_|YJwOOwj)~!:  `-`   ' - '` `+rixLTye#5$t?`-'  .. -.'. - .vO9)!!_::!<+>
//!/.. -.. +r{tLYTv4Mj<`-..iUZkVh6&gH9,-,. `  '. ``. ^--. --`- -`'.-'^~~` .`` - ` .`'  _*$dZ1<~,:+<
//,|t;!_'^=?{s1i)(*ri39?`_~^iPWgBQDEL''`'`  -.-. ' -  -``-`-  `-'.``~*,'`- ' -  ``-`. :vcTX5Y,..'::
//^:~)TYc(Y}r<;>>!,.`iU<` --` ;v{|:-`.`   ` -'  ..`-.`-'~ .. '`-`' .```__:`'` ---'`  ''~vDi  '.`.--
//<y<y*,AT"=.` `- ``';saT> --~ `'' ''`   '`-  '-   '-`"2ZF~  ` - `. ..'-`  . '     .`!<+(8(..- - ..
//,r=?X_ve="<.-'.  ` -' /tv<`~):- .' '  ``- '`'`. .+v1(5ZAv.`` '.--``-`- <:' . .`. !;{C55Ii ``'- ` 
//~j1(cv*C\;=""<++_~>~<>>7OMJ<+7l;i[\_ `-   '-'>\xs#aunnF$! . -`- .'..  -  `,!--- '-?MT:.-- '```.- 
//'^[jCT))Li}[i*r)*r()}upXl;LXy^~"^`;1Is1YT1YyzszY111Ytzfr     `- .  -.~>*;voyyYY[7vzk, '-` -``` -`
//`_"/=vTr7F4]}TkDS6$$Vt7|""+<vZ]^,'`'+15A5Jst111YTTTLTi^.`.` '. ``_>(Lf4XmAnt1Y7iiL7x ..  '`- ` `.
// <r;"r;?D" -`>MNTcviilivvrr><>26]+_ .'!v{T}c1xY1Yt{)^-- -- _!+<?[1jPw$1;:_`!`!^^ `'' .-. '''`--.'
//.<"*r=;i@=- `+mP#4PEVyY1cii|r<>=Yaq2]r<:__<;""?\|,:^,~>>=||i]YYTYMB5<~_^, ``-`` '      `.-`- '.'.
//'+*r">|EY'`- >OrsY!_;TYu1}]1YT2VAMOl)Ts#$e4Cyt1]7?|\?i[11}}y1yJJ24n#E++~~_, -'` -- --`  - '-   . 
// <;+=Y9;   - *S!_vJ:!*\rv7r~:,>F1v|+*/vxtvYXOy4@d*viivL1zsCtYLv"<>*vha<, .- ``` . .'   . `.''`---
//:vx7VJ,  .``-}5:!+iY_c52nvi{*^yf{<=tsFan?]$2;iiZOCyft21}\;<>:<)[Yoto2IXeZY~'-   -'''`  .-'. -``  
//`~!=>`.-. `->S1!~r1#v,44yy}>>i#|LkF(##nF3OAZ6q8Kqtr>+!^_!>iTzzzyJ#2(/)]Y1CXl< - `-`'.` `.`` ''  '
//-- `''.`' ^<T%=++ryFk>*J31i<,*5xvvX5TYl2N6VLlcyZMNv:!>?xstoFnfft6r(CJVIuSPzXSZ3ojnFY/+-'-.. `   '
//-`  . ` :>*rEn"<<=1C4j>/#yi>)s1|cXhOON&&a1vv}zCX6GOYiYfYs1ytzjCsk^q3P}_.,<1GVC2u1vv|isXf,  ~!..'`
//' ' ` _>*r"f$v);;*TFJ$i=LJvvec?JKMOD&@BXJtffu3XhPwO9vL1TfyfsfzVMO"FunAnY1yS31Yc)+?{ys+r9E, _~:^,_
//-..'-<>+=;76]iii?*YzoJa;=Z2T/o8MOMMDNN@4II2335hA99bMu7}YyfY1fPM8OP]fsYY1z1fT http://asciify.me !~
// - '_r>+=iSTii]}]LTJIo97#3(vZOOO&MMD&pFZPwkkXA%AP90M&1]1TfykOMpmMO8dG9wIfvr)xrxCeVIV5#zVM$;~,!>>^
      

