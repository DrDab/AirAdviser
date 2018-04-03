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

WiFiServer server(80);

SoftwareSerial pmsSerial(2, 3);

/**
 * Probe purchase link:
 * https://www.newegg.com/Product/Product.aspx?Item=9SIAEC96HY0339&ignorebbr=1&nm_mc=KNC-GoogleMKP-PC&cm_mmc=KNC-GoogleMKP-PC-_-pla-_-Gadgets-_-9SIAEC96HY0339&gclid=Cj0KCQjw1-fVBRC3ARIsAIifYOOhSSKaupNA857jX3l2fpRjNlefpp9aKfTHRs_bb6JLetQBgNtP1PcaAi93EALw_wcB&gclsrc=aw.ds
 * MCU + WiFi Chip purchase link:
 * https://www.amazon.com/HiLetgo-Internet-Development-Wireless-Micropython/dp/B010O1G1ES/
 * Solar powered battery pack:
 * https://www.amazon.com/Hiluckey-Waterproof-Shockproof-Travelling-Activities/dp/B075JGP36B/
 * Logic Level Converter (3.3V to 5V) :
 * https://www.sparkfun.com/products/12009
 * Total cost: $50
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

void loop() 
{
  if (readPMSdata(&pmsSerial)) 
  {
    // reading data was successful!
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
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client)
  {
    return;
  }
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
  s += "  background-color: skyblue;";
  s += "  color: white;";
  s += "}";
  s += "</style>";
  s += "</head>";
  if (val >= 0)
  {
    s += "<title>About</title>";
    s += "<strong>About</strong><br> ";
    s += "AirAdviser Beta v0.1 by Victor, Leodis and Carter";
    s += "<br>";
    s += "<a href=\"192.168.1.4/read\">Read sensor data</a>";
  }
  else if (val == -2)
  { 
    s += "<title>Sensor Reading</title>";
    s += "<strong>Pollution Sensor Data</strong>";
    s += "<br>";
    if (readPMSdata(&pmsSerial)) 
    {
      s += "PM1.0 (µg/m^3)";
      s += String(data.pm10_env);
      s += "<br>";
      s += "PM2.5 (µg/m^3)";
      s += String(data.pm25_env);
      s += "<br>";
      s += "PM10.0 (µg/m^3)";
      s += String(data.pm100_env);
      s += "<br>";
      if (data.pm25_env >= 500)
      {
        s += "<strong>DANGER: POLLUTION LEVELS HAZARDOUS!</strong>";
        s += "<br>";
        s += "PM2.5 Levels are over 500.0 µg/m^3";
        s += "<br>";
        s += "Do NOT go outdoors. If going outdoors, YOU risk:";
        s += "<br>";
        s += "<strong>- Serious aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly</strong>";
        s += "<br>";
        s += "<strong>- Serious risk of respiratory effects in general population. </strong>";
      } 
      else if (data.pm25_env < 500 && data.pm25_env >= 250)
      {
        s += "<strong>CAUTION: POLLUTION LEVELS VERY UNHEALTHY</strong>";
        s += "<br>";
        s += "PM2.5 Levels are over 250.0 µg/m^3";
        s += "<br>";
        s += "Do NOT go outdoors. If going outdoors, YOU risk:";
        s += "<br>";
        s += "<strong>- Significant aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly</strong>";
        s += "<br>";
        s += "<strong>- Significant increase in respiratory effects in general population. </strong>";
      }
      else if (data.pm25_env < 250 && data.pm25_env >= 150)
      {
        s += "<strong>CAUTION: POLLUTION LEVELS UNHEALTHY</strong>";
        s += "<br>";
        s += "PM2.5 Levels are over 150.0 µg/m^3";
        s += "<br>";
        s += "AVOID prolonged exertion outdoors. YOU risk:";
        s += "<br>";
        s += "<strong>- Increased aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly</strong>";
        s += "<br>";
        s += "<strong>- Increased respiratory effects in general population.</strong>";
      }
      else if (data.pm25_env < 150 && data.pm25_env >= 55)
      {
        s += "<strong>CAUTION: POLLUTION LEVELS UNHEALTHY FOR SENSITIVE PEOPLE</strong>";
        s += "<br>";
        s += "PM2.5 Levels are over 55.0 µg/m^3";
        s += "<br>";
        s += "AVOID prolonged exertion outdoors. YOU risk:";
        s += "<br>";
        s += "<strong>- Increased aggravation of heart or lung disease and premature mortality in persons with cardiopulmonary disease and the elderly</strong>";
        s += "<br>";
        s += "<strong>- Increased respiratory effects in general population.</strong>";
      }
      else
      {
        s += "<strong>POLLUTION LEVELS ARE HEALTHY</strong>";
      }
    }
    else
    {
      s += "No data yet";
      s += "<br>";
    }
    s += "<br>";
    s += "This page last updated at:";
    s += "<p id=\"time\"></p>";
    s += "<script>";
    s += "document.getElementById(\"time\").innerHTML = Date();";
    s += "</script>";
    s += "<a href=\"192.168.1.4/about\">About AirAdvisor</a>";
  }
  else
  {
    s += "ERROR 404 : Page Not Found.";
    s += "<br>";
    s += "<a href=\"192.168.1.4/read\">Read sensor data</a>";
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
      

