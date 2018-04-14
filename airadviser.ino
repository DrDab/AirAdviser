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
    o += "\"><br><br><br><br><br>Add me on FurAffinity: http://www.furaffinity.net/user/teslafox/";
    client.print(s+cloud_fox+o);
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
      

