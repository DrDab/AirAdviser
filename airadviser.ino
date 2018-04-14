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

// The Wi-Fi password for the hotspot
const char WiFiAPPSK[] = "noticemesenpai";

// Enable or disable serial debugging (will cause webserver slowdown)
const bool USE_SERIAL_DEBUGGING = false;

// Enable or disable warning LED
const bool USE_WARNING_LED = true;
const int WARNING_LED_PIN = 10; // GPIO10 corresponds to pin SD3 on NodeMCU

WiFiServer server(80);

SoftwareSerial pmsSerial(2, 3);

/**
 * Probe purchase link:
 * https://www.newegg.com/Product/Product.aspx?Item=9SIAEC96HY0339&ignorebbr=1&nm_mc=KNC-GoogleMKP-PC&cm_mmc=KNC-GoogleMKP-PC-_-pla-_-Gadgets-_-9SIAEC96HY0339&gclid=Cj0KCQjw1-fVBRC3ARIsAIifYOOhSSKaupNA857jX3l2fpRjNlefpp9aKfTHRs_bb6JLetQBgNtP1PcaAi93EALw_wcB&gclsrc=aw.ds
 * MCU + WiFi Chip purchase link:
 * https://www.amazon.com/HiLetgo-Internet-Development-Wireless-Micropython/dp/B010O1G1ES/
 * Solar powered battery pack:
 * https://www.amazon.com/Hiluckey-Waterproof-Shockproof-Travelling-Activities/dp/B075JGP36B/
 * Total cost: $40
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

boolean writeSerial = true;

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
        Serial.print("\t\tPM 10: "); Serial.println(data.pm100_standard);
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
    s += "<div>";
    s += "<p>Cloud Fox</p>";
    s += "<img src=\"data:image/png;base64,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\" alt=\"Cloud Fox\" /></div> ";
    s += "<br><br><br><br><br>Add me on FurAffinity: http://www.furaffinity.net/user/teslafox/";
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
      s += "<br>";
      s += "PM2.5 Level: ";
      s += String(pm25);
      s += " &mu;g/m^3";
      s += "<br>";
      s += "PM10.0 Level: ";
      s += String(pm100);
      s += " &mu;g/m^3";
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
      if (data.pm25_standard >= 500)
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
      else if (data.pm25_standard < 500 && data.pm25_standard >= 250)
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
      else if (data.pm25_standard < 250 && data.pm25_standard >= 150)
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
      else if (data.pm25_standard < 150 && data.pm25_standard >= 55)
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
        s += "<br>";
        s += "PM2.5 Level: ";
        s += String(pm25);
        s += " &mu;g/m^3";
        s += "<br>";
        s += "PM10.0 Level: ";
        s += String(pm100);
        s += " &mu;g/m^3";
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
  String AP_NameString = "AirAdviser-chan " + macID;

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
      

