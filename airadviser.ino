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

//////////////////////////////////////
// USER CONFIGURABLE SETTINGS BELOW //
//////////////////////////////////////

// The Wi-Fi SSID for the hotspot
const String WIFI_SSID = "AirAdviser-chan ";


// The Wi-Fi password for the hotspot
const char WiFiAPPSK[] = "noticemesenpai";

// Enable or disable serial debugging (will cause webserver slowdown)
const bool USE_SERIAL_DEBUGGING = false;

// Set pins for PMS5003 I/O
const int PMS_RX = 2;
const int PMS_TX = 3;

// Enable or disable warning LED
const bool USE_WARNING_LED = true;
const int WARNING_LED_PIN = 10; // GPIO10 corresponds to pin SD3 on NodeMCU

// Set port for HTTP webserver
const int HTTP_SERVER_PORT = 80;

//////////////////////////////////////
// END USER CONFIGURABLE SETTINGS   //
//////////////////////////////////////

WiFiServer server(HTTP_SERVER_PORT);

SoftwareSerial pmsSerial(PMS_RX, PMS_TX);

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
uint16_t pm10 = 0;
uint16_t pm25 = 0;
uint16_t pm100 = 0;

uint16_t p03 = 0;
uint16_t p05 = 0;
uint16_t p10 = 0;
uint16_t p25 = 0;
uint16_t p50 = 0;
uint16_t p100 = 0;

// deltas cannot be unsigned because negatives will cause overflow
int pm10_delta = 0;
int pm25_delta = 0;
int pm100_delta = 0;

// values for average calculation
int p10accum = 0;  int p10avg = 0;
int p25accum = 0;  int p25avg = 0;
int p100accum = 0; int p100avg = 0;

int ptrial = 0;

boolean writeSerial = true;

// Cloud Fox encoded image
const char* cloud_fox = "data:image/png;base64,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";

void loop() 
{
  runServer();
}

void runServer()
{
  boolean haveCurrentReading = false;
  if (readPMSdata(&pmsSerial)) 
  {
    hasReading = true;
    haveCurrentReading = true;
    
    // calculate the delta values.
    pm10_delta = (int)data.pm10_standard - (int)pm10;
    pm25_delta = (int)data.pm25_standard - (int)pm25;
    pm100_delta = (int)data.pm100_standard - (int)pm100;
    
    // calculate the averages.
    p10accum += (int)data.pm10_standard;
	  p25accum += (int)data.pm25_standard;
	  p100accum += (int)data.pm100_standard;
	  ptrial++;
	  p10avg = p10accum / ptrial;
	  p25avg = p25accum / ptrial;
	  p100avg = p100accum / ptrial;
	  if (ptrial == 500)
	  {
		  // reset every 500 trials to prevent integer overflow.
		  p10accum = 0;
		  p25accum = 0;
		  p100accum = 0;
		  ptrial = 0;
		  p10avg = 0;
		  p25avg = 0;
		  p100avg = 0;
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

  client.flush();

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
    o += "\">";
    client.print(s);
    client.print(cloud_fox);
    client.print(o);
    client.flush();
    delay(1);
    Serial.println("Client disconnected");
    return;
  }
  else if (val == -2)
  { 
    Serial.println("Reading page requested by client.");
    s += "<meta http-equiv=\"refresh\" content=\"2\" >";
    s += "<title>Sensor Reading</title>";
    s += "<strong>Pollution Sensor Data</strong>";
    s += "<br>";
    s += "========================================";
    s += "<br>";
    s += "Particulate Matter Concentrations";
    s += "<br>";
    if (haveCurrentReading) 
    {
      hasReading = true;
      Serial.println("Data collection successful, sending to client.");
      s += "PM1.0 Level: ";
      s += String(pm10);
      s += " &mu;g/m^3";
      s += " (&Delta;= ";
      s += String(pm10_delta);
      s += ")";
      s += "<br>";
      s += "PM2.5 Level: ";
      s += String(pm25);
      s += " &mu;g/m^3";
      s += " (&Delta;= ";
      s += String(pm10_delta);
      s += ")";
      s += "<br>";
      s += "PM10.0 Level: ";
      s += String(pm100);
      s += " &mu;g/m^3";
      s += " (&Delta;= ";
      s += String(pm10_delta);
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
      s += "<br><br>";
      s += "<table style=\"background-color: #000; border: 2px solid #00ff00; padding: 1px;\" cellpadding=\"0\" cellspacing=\"0\" align=\"center\">";
      s += "<tr>";
      s += "<td style=\"padding:2px\">";
      s += "<center>";
      s += "<big>";
      s += "<b>Warnings</b>";
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
        s += " (&Delta;= ";
        s += String(pm10_delta);
        s += ")";
        s += "<br>";
        s += "PM2.5 Level: ";
        s += String(pm25);
        s += " &mu;g/m^3";
        s += " (&Delta;= ";
        s += String(pm10_delta);
        s += ")";
        s += "<br>";
        s += "PM10.0 Level: ";
        s += String(pm100);
        s += " &mu;g/m^3";
        s += " (&Delta;= ";
        s += String(pm10_delta);
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
        s += "<br><br>";
        s += "<table style=\"background-color: #000; border: 2px solid #00ff00; padding: 1px;\" cellpadding=\"0\" cellspacing=\"0\" align=\"center\">";
        s += "<tr>";
        s += "<td style=\"padding:2px\">";
        s += "<center>";
        s += "<big>";
        s += "<b>Warnings</b>";
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
      }
    }
    s += "</center>";
    s += "</td></tr></table>";
    s += "<br>";
    s += "This page last updated at:";
    s += "<p id=\"time\"></p>";
    s += "<script>";
    s += "document.getElementById(\"time\").innerHTML = Date();";
    s += "</script>";
    s += "<a href=\"192.168.4.1/about\">About AirAdvisor</a>";
  }
  else
  {
    Serial.println("404 page requested by client.");
    s += "Call trans opt received: ";
    s += "<p id=\"time\"></p>";
    s += "<script>";
    s += "document.getElementById(\"time\").innerHTML = Date();";
    s += "</script>";
    s += "<br>";
    s += "Trace program: running";
    s += "<br><br>";
    s += "Wake up, Neo...";
    s += "<br>The Matrix has you.<br><br>";
    s += "ERROR 404 : Page Not Found.";
    s += "<br>";
    s += "<a href=\"192.168.4.1/read\">Read sensor data</a>";
  }
  s += "</html>\n";

  // Send the response to the client
  client.print(s);
  delay(1);
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
      

