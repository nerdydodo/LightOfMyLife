  /* LampServer - ESP8266 Webserver with lamp UI

   Based on ESP8266Webserver, DHTexample, and BlinkWithoutDelay (thank you)

   Version 1.0  20/09/2017  Version 1.0 
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
//#include <WiFiUdp.h>
//#include <NTPClient.h>


#include <FS.h>                   // File support to store settingd
#include "Adafruit_WS2801.h"      //LED strip

//#include "ParticleSys.h"
//#include "Particle_Std.h"
//#include "Particle_Fixed.h"
//#include "Particle_Attractor.h"
//#include "Emitter_Fountain.h"
//#include "PartMatrix.h"

#define RED_WHITEPOINT 255
#define GREEN_WHITEPOINT 158
#define BLUE_WHITEPOINT 120
#define MAX_INTENSITY 640

#define MAX(a,b) (a)>(b)?(a):(b)

typedef enum modus
{
  SOLID=0, 
  TWINKLE, 
  SPIRAL, 
  CHINESE_CRAP,
  FLAME
} Modus;

typedef enum colorModus
{
  SINGLE=0, 
  FIRE, 
  WILD_UNICORN,
  BIOHAZARD
} ColorModus;

typedef struct Color_
{
  int r;
  int g;
  int b;
} Color;

typedef struct LampState_
{
    int state;                 // 0/1 = on or off
    int modus;                 // SOLID, TWINKLE, SPIRAL, etc
    int colorModus;            // Fire, Unicorn, etc
    uint8_t intensity;         // Intensity multiplier of light
    uint8_t color_r;           // red value in case of solid 
    uint8_t color_g;           // green value in case of solid
    uint8_t color_b;           // blue value in case of solid
} LampState;

#define LAMPCONFIG_FILENAME "/lampConfig.txt"
const char* ssid     = "Entenhausen";
const char* password = "AllAnimalsAreEqual2017";

ESP8266WebServer server(80);
// WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 7200, 600000);

//#define UPDATES_PER_CYCLE 2
//#define NUM_PARTICLES 20

#define SPIRAL_LENGTH 23
const int numLeds = 40;
const uint8_t dataPin  = 5;                                                 // Yellow wire on Adafruit Pixels
const uint8_t clockPin = 4;                                                 // Green wire on Adafruit Pixels
Adafruit_WS2801 strip = Adafruit_WS2801(numLeds, dataPin, clockPin);        // Create the LED string object with 40 LEDs.

uint32_t   g_ledColors[numLeds];                                            // 24 bits color array stored in uin32_t
String     g_errorState="all okay";
int        g_commandCounter=0;
LampState  g_lampState;


/*                                  A1   A2   A3   A4   A5   A6   A7   A8   A9  A10  A11  C10   C9   C8   C7   C6   C5   C4    C3   C2   C1  C12   C11*/
// 31,  30,  29,  28,  27,  26,   25,  24,  23,  22,  33,  32
const int LampSpiral1[SPIRAL_LENGTH] = {   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  26,  27,  28,  29,  30,  31,  32,  33,  22,  23,  24,  25};
// 25,  24,  23,  22,  33,  32,   31,  30,  29,  28,  27,  26                                                                                                                                                                                      
/*                                  B1   B2   B3   B4   B5   B6   B7   B8   B9  B10  B11   C4   C3   C2   C1  C12  C11   C10  C9    C8   C7   C6   C5 */
const int LampSpiral2[SPIRAL_LENGTH] = {  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  32,  33,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31};

struct Color_ spiral1Display[SPIRAL_LENGTH]; 
struct Color_ spiral2Display[SPIRAL_LENGTH]; 

/* Used for particle system */
//Particle_Std         particles[numLeds];
//Particle_Attractor   particleSource;
//Emitter_Fountain     particleEmitter(0, 0, 5, &particleSource);
//ParticleSys          particleSys(numLeds, particles, &particleEmitter);
//PartMatrix           particleMatrix;

 /* helper function to find a sub-string (needle) in a long string (haystack) */
int findText(String needle, String haystack) {
  int foundpos = -1;
  for (int i = 0; i <= haystack.length() - needle.length(); i++) {
    if (haystack.substring(i,needle.length()+i) == needle) {
      foundpos = i;
    }
  }
  return foundpos;
}

/* return the value of a key-value pair. paramName is the search key */
String getValue(String data, String paramName)
{  
  int paramNamePos = findText(paramName, data);
  if (paramNamePos < 0) return "";
  
  int paramValueStart = paramNamePos+paramName.length()+1; // we want the value.. so skipi the param length and the '='
  int paramValueEnd   = data.indexOf("&", paramValueStart+1);
  
  return data.substring(paramValueStart, paramValueEnd);
}

/* helper function to serialize g_lampState */
String state2String()
{
  String paramString="";

  paramString  = "modus="+ String(g_lampState.modus)+"&";
  paramString += "colorModus="+ String(g_lampState.colorModus)+"&";
  paramString += "state="+ String(g_lampState.state) + "&";
  paramString += "intensity="+ String(g_lampState.intensity)+"&";
  paramString += "color_r="+ String(g_lampState.color_r)+"&";
  paramString += "color_g="+ String(g_lampState.color_g)+"&";
  paramString += "color_b="+ String(g_lampState.color_b)+"&";
  
  //TODO move the following to timer state
  //paramString += "days="+ String(g_lampState.days)+"&";
  //paramString += "start_minute="+ String(g_lampState.start_minute)+"&";
  //paramString += "end_minute="+ String(g_lampState.end_minute)+"&";
  //paramString += "timerEnabled="+ String(g_lampState.timerEnabled);
  
  return paramString; 
}

/* helper function to deserialize the g_lampState */
void string2State(String lampConfig)
{ 
  g_lampState.modus        = (Modus)getValue(lampConfig, "modus").toInt();
  g_lampState.colorModus   = (ColorModus)getValue(lampConfig, "colorModus").toInt();
  g_lampState.state        = getValue(lampConfig, "state").toInt();
  g_lampState.intensity    = getValue(lampConfig, "intensity").toInt();
  g_lampState.color_r      = (uint8_t)getValue(lampConfig, "color_r").toInt();
  g_lampState.color_g      = (uint8_t)getValue(lampConfig, "color_g").toInt();
  g_lampState.color_b      = (uint8_t)getValue(lampConfig, "color_b").toInt();

  //TODO move these to timer
  //g_lampState.days         = (uint8_t)getValue(lampConfig, "enabledDays").toInt();
  //g_lampState.start_minute = getValue(lampConfig, "start_minute").toInt();
  //g_lampState.end_minute   = getValue(lampConfig, "end_minute").toInt();
  //g_lampState.timerEnabled = getValue(lampConfig, "timerEnabled").toInt()==1;
}

/* Load the lamp state from file  */
void loadState()
{
  File f = SPIFFS.open(LAMPCONFIG_FILENAME, "r");
  if (!f) 
  {
    g_errorState = "Could not open file";
  }
  else
  {
    String lampConfig = f.readStringUntil('\n');
    string2State(lampConfig);
    g_errorState="File loaded: <br>";
    g_errorState+=lampConfig + "\n";
    f.close();
  }
}

/* Store the lamp state to a file */
void storeLampState()
{
  String saveString = state2String();
  File f = SPIFFS.open(LAMPCONFIG_FILENAME, "w");
  if (f)  
  {
    f.println(saveString);
    g_errorState="save okay";
  }
  else 
  {
    g_errorState="Error: could not update config file";
  }
  f.close();  
}


/* web server functions */
void handleRoot() /* no command given, report system is online */
{  
    server.send(200, "text/html", "System online"); 
}

void handleError() /* request for the error string */ 
{
    server.send(200, "text/html", g_errorState);
}

void handleDebug() /* display what is going on */
{
  String debugText = ""; //"<head><meta http-equiv=\"refresh\" content=\"5\" /></head><body>";
  
  debugText += "<p style=\"color:rgb(0,64,200)\">Lamp state: " + state2String() + "</p><br>\n"; 
  loadState();
  debugText += "Error is: {" + g_errorState + "}<br>\n";
  debugText += "Number of commands processed: "+ String(g_commandCounter) + "<br>\r <br>\r <br>\rlampSpiral1 indices: "; 
  for (byte x=0;x<SPIRAL_LENGTH;x++)
  {
    debugText += String(LampSpiral1[x]) + ",";
  }
  debugText += "<br>\rlampSpiral2 indices: ";
  for (byte x=0;x<SPIRAL_LENGTH;x++)
  {
    debugText += String(LampSpiral2[x]) + ",";
  }
  debugText += "<br>/r<table background=\"black\">";
  
  if (SPIRAL) //2x23
  {
    debugText += "<tr>";
    for (byte x=0;x<numLeds;x++)
    {
      char colorString[128];
      sprintf(colorString, "#%x", strip.getPixelColor(x));       
      debugText += "<td bgcolor=\"" + String(colorString) + "\"><div style=\"" + String(colorString) + "\"><h1>*</h1></div></td>\n";
    }
    debugText += "</tr><tr>";
    for (byte x=0;x<numLeds;x++)
    {
      char colorString[128];
      sprintf(colorString, "#%x", strip.getPixelColor(x));      
      debugText += "<td bgcolor=\"" + String(colorString) + "\"><div style=\"" + String(colorString) + "\"><h1>*</h1></div></td>\n";
    }
    debugText += "</tr>";
  }
  debugText += "</table>";  

  debugText += "</body>";   
  server.send(200, "text/html", debugText); 
}

/* Command to force saving current state */
void handleSave() /* sace */
{
  storeLampState();
  server.send(200, "text/html", "saved something");  
}

/* Command to reload current state */
void handleLoad() /* load */
{
    loadState();
    server.send(200, "text/html", g_errorState);  
}

/* handle a lamp command by parsing the input paramteres */
void handleLamp() /* lamp */
{
  String responseString = ""; 
  g_commandCounter++;
  
  if (server.arg("state")!= "")
  {
    g_lampState.state = server.arg("state").toInt();
    responseString+="state=" + server.arg("state") + "&"; 
  }
  if (server.arg("modus")!= "")
  {
    g_lampState.modus = (Modus)server.arg("modus").toInt();
    responseString+="modus=" + server.arg("modus")+ "&"; 
  }
  if (server.arg("colorModus")!= "")
  {
    g_lampState.colorModus = (ColorModus)server.arg("colorModus").toInt();
    responseString+="colorModus=" + server.arg("colorModus")+ "&"; 
  }
  if (server.arg("color_r")!= "")
  {
    g_lampState.color_r = (uint8_t)server.arg("color_r").toInt();
    responseString+="color_r=" + server.arg("color_r")+ "&";
  }
  if (server.arg("color_g")!= "")
  {
    g_lampState.color_g = (uint8_t)server.arg("color_g").toInt();
    responseString+="color_g=" + server.arg("color_g")+ "&";
  }
  if (server.arg("color_b")!= "")
  {
    g_lampState.color_b = (uint8_t)server.arg("color_b").toInt();
    responseString+="color_b=" + server.arg("color_b")+ "&";
  }
  if (server.arg("intensity")!= "")
  {
    g_lampState.intensity = (uint8_t)server.arg("intensity").toInt();
    responseString+="intensity=" + server.arg("intensity")+ "&";
  }
  /*
  if (server.arg("days")!= "")
  {
    g_lampState.days = (uint8_t)server.arg("days").toInt();
    responseString+="days=" + server.arg("days")+ "&";
  }
  if (server.arg("start_minute")!= "")
  {
    g_lampState.start_minute = (int)server.arg("start_minute").toInt();
    responseString+="start_minute=" + server.arg("start_minute")+ "&";
  }
  if (server.arg("end_minute")!= "")
  {
    g_lampState.end_minute = (int)server.arg("end_minute").toInt();
    responseString+="end_minute=" + server.arg("end_minute")+ "&";
  }   
  if (server.arg("timerEnabled")!= "")
  {
    g_lampState.timerEnabled = (server.arg("true").toInt())!=0;
    responseString+="timerEnabled=" + server.arg("timerEnabled")+ "&";
  } 
  */
  storeLampState();
  server.send(200,"text/plain", "okidokie: " + responseString);
}
 
void setup(void)
{
  int i;

  /* 1)  Initialize the strip */
  strip.begin();
  delay(200);            // wait for power 
  strip.setPixelColor(0,250,0,125); //purple/pink-ish 
  strip.show();

/*
  particleSource.vx = 3;
  particleSource.vy = 1;
  particleSource.x = random(50)+100;
  particleSource.y = random(10)+1; //10+1
  Particle_Std::ay = 2; //2
  PartMatrix::isOverflow = false;
  Emitter_Fountain::minLife = 100;  //100
  Emitter_Fountain::maxLife = 250; //200
  ParticleSys::perCycle = 2;  //2
  Particle_Attractor::atf = 2;  //2
*/  
  /* 2) Serial You can open the Arduino IDE Serial Monitor window to see what the code is doing */
  Serial.begin(115200);  // Serial connection from ESP-01 via 3.3v console cable, only use it in case there is no network 
  delay(400);            // 200ms delay to get LED strip stable

  /* 3) Start SPIFFS and load the state */
  bool result = SPIFFS.begin();
  loadState();
  
  /* 4) Connect to WiFi network, reboot every 5 seconds until connected  */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) 
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  /* 5) Inituialize the OTA functions */
  ArduinoOTA.onStart([]() {
    //Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  /* we are online, print some info to the serial monitor to know what is going on when the network would fail */
  Serial.println("Ready");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  strip.setPixelColor(0,250, 0, 0); //turn on a LED make it green 
  strip.show();

  /* 6) Start the time client */
  // timeClient.begin();     

  /* 7) install the callbacks */ 
  server.on("/",      handleRoot);
  server.on("/lamp",  handleLamp);
  server.on("/debug", handleDebug);
  server.on("/load",  handleLoad);
  server.on("/save",  handleSave);
  server.on("/error", handleError);
  server.begin();  
}

/* Create a 24 bit color value from R,G,B */
uint32_t encodeColor(byte r, byte g, byte b)
{
  uint32_t c;
  c = r;
  c <<= 8;
  c |= g;
  c <<= 8;
  c |= b;
  return c;
}

/* calculate the light on a spiral */
#define MIN_BLUE 10
void setFilteredPos(Color *spiral, float pos, int red, int green, int blue)
{
  int intPos = (int)floor(pos);
  float fragment = pos - ((float)(intPos));
  // linear interpolation (2-taps low and high)
  float red_low = (float)red*fragment;                   
  float red_high = red*(1.0-fragment);  

  float green_low = (float)green*fragment;
  float green_high = green*(1.0-fragment); 

  float blue_low = (float)blue*fragment;
  float blue_high = blue*(1.0-fragment); 

  if (intPos < 0 || intPos >= SPIRAL_LENGTH+3) 
  {
    spiral[0].r = 0;
    spiral[0].g = 0;
    spiral[0].b = 255; //blue to signal error
    return;
  }
 
  if (intPos > 2 && (intPos-3) < SPIRAL_LENGTH)
  {
    spiral[intPos-3].r = 0;
    spiral[intPos-3].g = 0;
    spiral[intPos-3].b = MIN_BLUE;
  }
  if (intPos > 1 && (intPos-2) < SPIRAL_LENGTH)
  {
    spiral[intPos-2].r = red_high;
    spiral[intPos-2].g = green_high;
    spiral[intPos-2].b = MAX(blue_high, MIN_BLUE);
  }
  if (intPos > 0 && (intPos-1) < SPIRAL_LENGTH)
  {
    spiral[intPos-1].r = red;
    spiral[intPos-1].g = green;
    spiral[intPos-1].b = MAX(MIN_BLUE,blue);
  } 
  if (intPos < SPIRAL_LENGTH)
  {
    spiral[intPos].r = red_low;
    spiral[intPos].g = green_low;
    spiral[intPos].b = MAX(blue_low, MIN_BLUE);
  }
}

/* render the spiral on the LED string */
void renderSpiral(Color *spiralDisplay, const int *spiral)
{
  for (int i=0; i < SPIRAL_LENGTH; i++)
  {
    uint32_t currentColor = strip.getPixelColor(spiral[i]);
    //todo handle clipping!
    uint32_t addedColor = currentColor + encodeColor(spiralDisplay[i].r, spiralDisplay[i].g, spiralDisplay[i].b);
    strip.setPixelColor(spiral[i], addedColor);
  }
}

/* main LED string */
void updateLedStrip()
{
  int i,r;
  static float spiralPos1=0.05f;
  static float spiralPos2=0.25f;
  static int spiralBaseIntensity=0;
   
  int intensity = g_lampState.state?g_lampState.intensity>100? 100:g_lampState.intensity:0;
  int red   = ((g_lampState.color_r>255?255:g_lampState.color_r) * intensity*(intensity+10) * RED_WHITEPOINT)/  2805000; //(110 *255*100)=2805000
  int green = ((g_lampState.color_g>255?255:g_lampState.color_g) * intensity*(intensity+10) * GREEN_WHITEPOINT)/2805000;
  int blue  = ((g_lampState.color_b>255?255:g_lampState.color_b) * intensity*(intensity+10) * BLUE_WHITEPOINT)/ 2805000;
  long updateValue;
  
  switch (g_lampState.modus)
  { 
    case SOLID:
      for (i=0; i<strip.numPixels(); i++) 
      {
        strip.setPixelColor(i,red,green,blue);
      }  
      break;
    case TWINKLE:
      updateValue = random(0, 120);
      if (updateValue>60)
      {
        long pos = random(0, strip.numPixels());
        strip.setPixelColor((uint16_t)pos, red, green, blue);
      }        
      for (i=0; i<strip.numPixels(); i++) 
      {
        uint32_t codedColor = strip.getPixelColor(i);
        int codedColorRed   = (codedColor>>16)&0xff;
        int codedColorGreen = (codedColor>>8)&0xff;
        int codedColorBlue  = (codedColor)&0xff;
        
        if (codedColorRed > 0) codedColorRed--;
        if (codedColorGreen > 0) codedColorGreen--;
        if (codedColorBlue > 0) codedColorBlue--;
        strip.setPixelColor(i, codedColorRed, codedColorGreen, codedColorBlue); 
      }      
      break; 
    case SPIRAL:
      for (i=0; i<strip.numPixels(); i++) 
      {
        strip.setPixelColor(i,0,0,0);
      }
      if (spiralPos1 > (float)SPIRAL_LENGTH+3.0f) spiralPos1 = 0.0f;
      if (spiralPos2 > (float)SPIRAL_LENGTH+3.0f) spiralPos2 = 0.0f;    

      setFilteredPos(spiral2Display, spiralPos2, red, green, blue); 
      setFilteredPos(spiral1Display, spiralPos1, red, green, blue);

      if (spiralPos1 > 17 || spiralPos2 > 17) spiralBaseIntensity++;      
      else if (spiralBaseIntensity > 0) spiralBaseIntensity-=4;

      if (spiralBaseIntensity < 0) spiralBaseIntensity=0;
      if (spiralBaseIntensity > MAX_INTENSITY) spiralBaseIntensity=MAX_INTENSITY;
   
      for (i=34; i<strip.numPixels(); i++) 
      {
        strip.setPixelColor(i,(red*spiralBaseIntensity)/MAX_INTENSITY,
                              (green*spiralBaseIntensity)/MAX_INTENSITY,
                              MAX((blue*spiralBaseIntensity)/MAX_INTENSITY,MIN_BLUE));
      }
      
      renderSpiral(spiral1Display, LampSpiral1);
      renderSpiral(spiral2Display, LampSpiral2);          
      spiralPos1 += 0.025f;
      spiralPos2 += 0.025f;
      
      break;
    case CHINESE_CRAP:
      for (i=0; i<strip.numPixels(); i++) 
      {
        strip.setPixelColor(i,random(0, 255),random(0,255),random(0,255));
      }
      break;
    case FLAME:
      //drawParticles();
      break;
  }
}
 
void loop(void)
{
  delay(10);               // delay for frame rate control
  updateLedStrip();        // update the lamp LEDs
  strip.show();            // write all the pixels out, this takes 1 ms for latching
//  particleSys.update();
  ArduinoOTA.handle();    // handle Over The Air programming requests
  server.handleClient();  // Listen for HTTP requests from clients
//  timeClient.update();    // update the clock 
} 
