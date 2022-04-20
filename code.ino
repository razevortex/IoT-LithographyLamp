LIT RGB Project (1st Stable)
/*

  INCLUDE LIBs DEFINE SOME VARIABLES

*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
ESP8266WebServer server(80);
#include <FastLED.h>
#define LED_PIN     2
#define NUM_LEDS    60
CRGB leds[NUM_LEDS];
#define STEPPER_A  14
#define STEPPER_B  12
#define STEPPER_C  13
#define STEPPER_D  15
#define STEPS_PER_ROT 4096

/*

  INITIATE GLOBAL VARs

*/

//              STEPPER

// Stepper arrays to group the pins and a halfstep control pattern
const int stepper_pin[4] = {STEPPER_A, STEPPER_B, STEPPER_C, STEPPER_D};

/* These are used to get the input value determine if it rotates clockwise and the rpm 
 * im aware that to get real rpm isnt necessary but its here because of a planed feature not implemented yet. 
*/
signed int rotation = 0;  // receives converted HTML val
signed int rotation_old = 0;  // checks for changes if new interval calculation is needed
byte rpm;
byte pattern_cycle = 0; //this is the pattern used right now
int pattern_of_rotation = 0; // this counts along the STEPS_PER_ROT for accurate rpm
volatile bool clockwise = true;

/*   
 *   pattern (be aware its the real amount of pattern not -1) via a mod calculation used to determine the next pattern
 *   its here to make changes to the pattern (going for fullstep etc) easier
 */
int pattern = 8;
const int stepper_pattern[8][4] = {
  {1, 1, 1, 0}, {1, 1, 0, 0}, {1, 1, 0, 1}, {1, 0, 0, 1},
  {1, 0, 1, 1}, {0, 0, 1, 1}, {0, 1, 1, 1}, {0, 1, 1, 0}
};

// timer values for stepper
int stepper_interval;
unsigned long stepper_timer;


//                LED STRIPE

// Led related arrays
byte rgb[3] = {0, 0, 0}; // rgb holds the realtime value
int w;  // this receives the alpha from html input is used for converting didnt got rit of it yet
float rgb_alpha = 0.5;   // multiplied with alpha ( brightness ) it is used to control the leds 

// These are needed for calculate the "now" rgb values of the fade mode 
byte rgb_start[3] = {0, 0, 0};
float to_finish;
signed int rgb_distance[3] = {0, 0, 0};

// keyframes are used to get pick the next rgb values via random() last and next needed to avoid repeating same keyframe
byte last_keyframe = 0;
byte next_keyframe = 0;
byte keyframes_total = 19;

/* i could have used random() to get the rgb values itself but because the random() isnt as random as i would like 
 * the way to give a set of fixed values to pick from gave better results for me...
 * anyway the rgb_fade_keyframe can easily be edited just remember to adjust the keyframe total.
*/
byte rgb_fade_keyframe[20][3] =  {
  {254, 0, 0}, {254, 127, 0}, {254, 127, 127}, {0, 254, 0,}, {0, 254, 127},
  {127, 254, 127}, {0, 0, 254}, {127, 0, 254}, {127, 127, 254}, {254, 254, 0},
  {127, 254, 0}, {254, 127, 254}, {0, 254, 254}, {0, 127, 254}, {254, 254, 127},
  {254, 0, 254}, {254, 0, 127}, {127, 254, 254}, {95, 95, 95}, {192, 192, 192}
};
// the timer vals that are necessary for the delay free fade function and checking for changes
int fade_interval = 0;
int fade_interval_old = 0;
unsigned long fade_timer;


//              HTML & EEPROM

// is read/write of data from/to EEPROM requested (by the html save function)
volatile bool store_data = false;
volatile bool get_data = false;

// is read/write of data from/to EEPROM requested (for the last settings used)
volatile bool store_last=false;
volatile bool get_last = true; // this is true to get last vals after start

// the input values to hold and return them to the HTML page  
String string0 = "127";
String string1 = "127";
String string2 = "127";
String string3 = "0";
String string4 = "0";
String string5 = "0";

// used to string together the values seperated via "$" for write to EEPROM 
String dataString = "";
// to get the indexOf the used "$" seperater and substring each value
int stringMarker[7];

// the html code of the <head> + banner of the <body> its always used cause it doesnt contain changeing vals 
const char HEAD_HTML[] =

  " <html><head><title>LitRGB</title><meta name=\"viewport\" content=\"width=device-width, user-scalable=yes, initial-scale=1, maximum-scale=2\"><meta name=\"author\" content=\"Raze VorteX\"> "
  /* CSS  */
  "<style>"
  "body {background-color:rgba(0,0,0,0.5); padding:2%; width:96%;}"
  ".container {background-color: rgba( 0,0,0, 0.7); width:100%; border:solid;min-height:100 px; border-radius:4em;text-align:center;}"
  ".box { width:96%; margin:2%;  border-style: solid; border-radius :2em;}"
  "div.banner {background-image: linear-gradient(to right,rgba(254,254,254, 0.3),rgba(0,0,0,0.6),rgba(0,0,0,0.6),rgba(254,254,254,0.3)); width:100%; border-style: solid; border-radius :2em;}"
  "h2 {color:white; align: center; } p{color:white; text-shadow: 2px 2px grey; align:center;} .button p{color:white; font-size: 13px; align:center;}.info p{color:black ; font-size: 13px; align:center;}"
  "#head { text-align:center; }"
  ".slider{ appearance: none;  background: rgba(0,0,0,0.0);width: 100%;  outline: none; opacity: 0.7; transition: opacity 0.2s ease 1s; cursor: pointer; align:center;}"
  ".slider_r {position:relative; left:10%; background-image: linear-gradient(to right,rgba(254,0,0, 0.0),rgba(254,0,0,1.0)); width:80%; border:solid; height:25px; border-radius:4em;}"
  ".slider_g {position:relative; left:10%; background-image: linear-gradient(to right,rgba(0,254,0, 0.0),rgba(0,254,0,1.0)); width:80%; border:solid; height:25px; border-radius:4em;}"
  ".slider_b {position:relative; left:10%; background-image: linear-gradient(to right,rgba(0,0,254, 0.0),rgba(0,0,254,1.0)); width:80%; border:solid; height:25px; border-radius:4em;}"
  ".slider_w {position:relative; left:10%; background-image: linear-gradient(to right,rgba(254,254,254, 0.0),rgba(254,254,254,0.8)); width:80%; border:solid; height:25px; border-radius:4em;}"
  ".slider_m {position:relative; left:10%; background-image: linear-gradient(to right,rgba(254,254,254, 0.8),rgba(0,0,0,0.0),rgba(254,254,254, 0.8)); width:80%; border:solid; height:25px; border-radius:4em;}"
  ".slider_f {position:relative; left:10%; background-image: linear-gradient(to right,rgba(0,0,0, 0.8),rgba(254,0,0, 0.8),rgba(0,254,0,0.8),rgba(0,127,127, 0.8),rgba(0,63,191, 0.8),rgba(0,0,254, 0.8)); width:80%; border:solid; height:25px; border-radius:4em;}"
  ".button input { appearance: none; color:white; background: rgba(127,127,127,0.5);width: 80%; align:center; min-height:30px; border:solid; border-radius:4em;}"
  ".container button{ appearance: none; float:left;  background: rgba(127,127,127,0.5);width: 40%; margin-left:5%;margin-right:5%; align:center; border:solid;border-color:white; border-radius:4em;}"
  ".info{position:absolute; right:2%; top:3%;}"
  ".info button {background: rgba(191,191,191,0.9);border:solid;border-color:white; border-radius:4em; z-index:1;}"
  ".green{color: green;align: center;}"
  ".red    {color: red;align: center;}"
  ".blue   {color: blue;align: center;}"
  "</style></head> "
  /* Banner  */
  "<body>"
  "<div class=\"banner\" id=\"head\"><a href=\"/\" >"
  "<h2>Lit-<span class=\"red\">R</span><span class=\"green\">G</span><span class=\"blue\">B</span></h2></a></div><br>" ;


//                  AP SERVER

// i was planning on implement a proper setup for enter custom ssid and password
// and also a option to enter login infos for home wlan to make esp run in station mode too.
// but didnt got the time yet so just the default
char* ssid = "LitRGB";   


//                  GENERAL VAL

// you know what this is FOR ;D
byte i;

//          SETUP

void setup() {
  Serial.begin(115200);
  // Initiate random for fade function
  randomSeed(analogRead(0));
  // Initiate the FastLED library
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  // starts esp´s ap mode
  WiFi.softAP(ssid);
  // set stepper pins to output
  for (i = 0; i <= 3; i++) {
    pinMode(stepper_pin[i], OUTPUT);
  }
  // initiate EEPROM with the necassery size
  EEPROM.begin(64);
  // set the needed Server handles
  server.on("/", handleRoot);
  server.on("/action_page", handleForm);
  server.begin();
}

/*

  EEPROMs MAIN FUNCTIONs
  that execute GET & STORE
  of the data

*/

//        EEPROM STORE FUNCTION used for execution

void store2EEPROM() {
  // check on what area it was requested to store and set address
  if ((store_data)||(store_last)) {
    int address=(store_last==true? 0:32);

    // String together the data that is going to be stored
    dataString = "$";
    dataString += string0;
    dataString += "$";
    dataString += string1;
    dataString += "$";
    dataString += string2;
    dataString += "$";
    dataString += string3;
    dataString += "$";
    dataString += string4;
    dataString += "$";
    dataString += string5;
    dataString += "$";

    // give string and address to the function that does the writing
    writeStringToEEPROM(address, dataString);

    // set the request bools back to false
    store_data = false;
    store_last = false;
  }
}

//        EEPROM STORE FUNCTION used for execution
void get_store2EEPROM() {
  // check on what area it was requested to store and set address
if ((get_data)||(get_last)){
int address=(get_last==true? 0:32);
  // get the stored String
  dataString = readStringFromEEPROM(address);
  // get the indexOf() the serperators
  stringMarker[0] = dataString.indexOf('$');
  for (i = 1; i < 7; i++) {
    stringMarker[i] = dataString.indexOf('$', stringMarker[i - 1] + 1);
  }
  // substring each string between them
  string0 = dataString.substring(stringMarker[0] + 1, stringMarker[1]);
  string1 = dataString.substring(stringMarker[1] + 1, stringMarker[2]);
  string2 = dataString.substring(stringMarker[2] + 1, stringMarker[3]);
  string3 = dataString.substring(stringMarker[3] + 1, stringMarker[4]);
  string4 = dataString.substring(stringMarker[4] + 1, stringMarker[5]);
  string5 = dataString.substring(stringMarker[5] + 1, stringMarker[6]);
  // convert the values and give it the according variable
  rgb[0] = string0.toInt();
  rgb[1] = string1.toInt();
  rgb[2] = string2.toInt();
  w = string3.toInt();
  rotation = string4.toInt();
  fade_interval = string5.toInt();
  fade_interval *= 500;
  rgb_alpha = (float)w / 100;
}
// if data are loaded request to store them as last settings 
store_last=(get_data==true? true:false);
// end get requests
get_data=false;
get_last=false;
}

/*

  EEPROMs SUB FUNCTIONs
  that write & commit stored and
  read, substring & convert gathered data

*/  

// this function writes the received data tp EEPROM and commits(only needed on ESP cause its in fact an emulated eeprom on its flash memory)it.
void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  // checks the length of the given string
  byte len = strToWrite.length();
  // and writes the lenght value on index 0 of the address
  EEPROM.write(addrOffset, len);
  // writes the rest of the string
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + i + 1, strToWrite[i]);
  }
  EEPROM.commit();
}

// it reads the stored data and returns the data as a string
String readStringFromEEPROM(int addrOffset)
{
  // checking lenght of the stored string ( mentioned in the writing function above )
  int newStrLen = EEPROM.read(addrOffset);
  // initialize a char array for the data and stores it 
  char data[newStrLen];
  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + i + 1);
  }
  data[newStrLen] = '\0';
  // converts the char array to string and returns it
  dataString = String(data);
  return dataString;
}


/*

  HANDLE DATA AND
  INITIALIZING of the MAIN FUNCTIONs
  for STEPPER AND LED

*/

//        INITIALIZE STEPPER handles changed input vals
void init_stepper() {
  // checks for changed rotation vals
  if (rotation != rotation_old) {
    rotation_old = rotation;
    // checks if input is negative to set turn direction  
    if (rotation > 0) {
      clockwise = true;
      //if possitive gives value to rpm
      rpm = rotation;
    }
    else if (rotation < 0) {
      clockwise = false;
      // if negative gives is negative value ( then possitive ) to rpm
      rpm = rotation * (-1);
    }
    // calculate the interval for the according rpm val
    stepper_interval = 60000 / (rpm * STEPS_PER_ROT);
    stepper_timer = millis();
  }
}

//        INITIALIZE RGB FADEING handles changed input vals
void init_rgbfade() {
  // checking for changed fade interval value
  if (fade_interval != fade_interval_old) {
    // if value isnt 0 (turns fade mode off)
    if (fade_interval != 0) {
      // starts the function that gives next keyframe and starts a new fade cycle
      get_fade_val();
    }
    fade_interval_old = fade_interval;
  }
}

//        FUNCTION FOR START FADE CYCLE
void get_fade_val() {
  //gives a next keyframe till its not the same as the last one
  while (last_keyframe == next_keyframe) {
    next_keyframe = random(0, keyframes_total);
  }
  last_keyframe = next_keyframe;
  // set the according values
  for (i = 0; i <= 2; i++) {
    // where the fade cycle starts
    rgb_start[i] = rgb[i];
    // and the way it goes ( if up or down and how far )
    rgb_distance[i] = rgb_fade_keyframe[next_keyframe][i] - rgb[i];
  }
  // starts the timer for the cycle
  fade_timer = millis();
}

//            CALCULATES THE RGB VALUES OF FADE CYCLE TO THE GIVEN TIME
void rgb_fade() {
  // if the fade cycle is still running
  if (millis() - fade_timer < fade_interval) {
    // check how long its running already
    int fade_time = millis() - fade_timer;
    // and get a float value of its progress
    to_finish = (float)fade_time / (float)fade_interval;
    // by knowing where it started and how far of its way its went so far we get the NOW value by adding it together 
    for (i = 0; i <= 2; i++) {
      signed int step_val = rgb_distance[i] * to_finish;
      rgb[i] = rgb_start[i] + step_val;
    }
  }
  // if the fade timer did run out already we just get next keyframes and next cycle starts
  else {
    get_fade_val();
  }
}

/*

  MAIN FUNCTION´s that execute/control
  the outputs for STEPPER AND LED

*/

//              MAIN FUNCTION CONTROLING STEPPER
void stepper() {
  init_stepper();
  // check if rotation isnt 0 ( means if it should move)
  if (rotation != 0) {
    // while it isnt at its step it needs to be by now
    while (millis() - stepper_timer > stepper_interval) {
      // the nexts steps pattern gets calculated
      pattern_cycle = pattern_of_rotation % pattern;
      // and executed
      for (i = 0; i <= 3; i++) {
        digitalWrite(stepper_pin[i], stepper_pattern[pattern_cycle][i]);
      }
      // keeping track of what step during a 360° rotation it is right now
      if (clockwise) {
        pattern_of_rotation++;
        pattern_of_rotation = (pattern_of_rotation > STEPS_PER_ROT ? 0 : pattern_of_rotation);
      }
      else {
        pattern_of_rotation--;
        pattern_of_rotation = (pattern_of_rotation < 0 ? STEPS_PER_ROT : pattern_of_rotation);
      }
      // adding the interval duration to timer instead instead of give a new milli() 
      // to allow it to compensate time and doesnt fall behind ( the interval at 7 rpm in my case is about 2ms )
      stepper_timer += stepper_interval;
    }
  }
  // if its rotation value is 0 then turn all pins low ( wasnt needed to fix stepper in its possition in this project )
  // to prevent heating and save energy
  else if (rotation == 0) {
    for (i = 0; i <= 3; i++) {
      digitalWrite(stepper_pin[i], LOW);
    }
  }
}

//        MAIN FUNCTION CONTROLLING LEDS
void led_control() {
  // each current rgb value multiplicated with the alpha given to the library function done
  for (i = 0; i < NUM_LEDS; i++) {
    leds[i].setRGB(rgb[0]*rgb_alpha, rgb[1]*rgb_alpha, rgb[2]*rgb_alpha);
  }
  FastLED.show();
}

/*

  HTML CODE / SERVER REQUEST/HANDLE FUNCTION´s

*/

//          HANDLE ROOT PAGE (page for enter input)
void handleRoot() {
  // get the HEAD HTML string and add the rest of the HTML code to it
  String message = HEAD_HTML;
  /*RGB Value Slider*/
  message += "<div class=\"container\"><p> RGB und Helligkeit </p> <form action=\"/action_page\" >";
  message += "<div class=\"slider_r\">";
  message += "<input type=\"range\" id=\"r\" class=\"slider\" name=\"r\" value=\"";
  // add the strings with the current set values of the according sliders 
  message += string0;
  message += "\" min=\"0\" max=\"254\">";
  message += "</div><br><div class=\"slider_g\">";
  message += "<input type=\"range\" id=\"g\" class=\"slider\" name=\"g\" value=\"";
  message += string1;
  message += "\" min=\"0\" max=\"254\">";
  message += "</div><br><div class=\"slider_b\">";
  message += "<input type=\"range\" id=\"b\" class=\"slider\" name=\"b\" value=\"";
  message += string2;
  message += "\" min=\"0\" max=\"254\">";
  message += "</div><br><div class=\"slider_w\">";
  message += "<input type=\"range\" id=\"w\" class=\"slider\" name=\"w\" value=\"";
  message += string3;
  message += "\" min=\"0\" max=\"100\" >";
  message += "</div><br>";
  /* Motor Speed Slider */
  message += "<p> Rotation Speed </p>";
  message += "<div class=\"slider_m\">";
  message += "<input type=\"range\" id=\"rotation\" class=\"slider\" name=\"rotation\" min=\"-7\" max=\"7\" value=\"";
  message += string4;
  message += "\"></div>";
  /* Fade Slider */
  message += "<br><p> Fadeing RGB</p>";
  message += "<div class=\"slider_f\">";
  message += "<input type=\"range\" id=\"fadeint\" class=\"slider\" name=\"fadeint\"  value=\"";
  message += string5;
  message += "\"min=\"0\" max=\"20\" >";
  // Submit button for the form input vals
  message += "</div><br><div class=\"button\">";
  message += "<input type=\"submit\" value=\"Submit\"></div> </form></div><br>";
  // 2 submit button of another form that give get the value 1 to the action page 
  // the requested action is then picked by the arguments name
  message += "<div class=\"container\"><p> SAVE/LOAD FAV: </p><form action=\"/action_page\" >";
  message += "<button type=\"submit\" class=\"sl\" name=\"saveVals\" value=\"1\"><p>Save</p></button>";
  message +=  "<button type=\"submit\" class=\"sl\" name=\"loadVals\" value=\"1\"><p>Load</p></button> </form><br><br><br></div>";
  message += "</body></html>";
  // send the page
  server.send(200, "text/html", message);
}

//          HANDLE ACTION_PAGE (page that checks for GET arguments)
void handleForm() {
  // again starting with the head
  String message = HEAD_HTML;
  // add rest of the page that is basicly just a link to go back to root again
  message += "<div class=\"container\"><a href=\"/\" ><br><p>request done</p><br>";
  message += "<p>*click banner to go back*</p><br></a></div>";
  server.send(200, "text/html", message);
  // checks if the first form was used ( checking for one of its arguments would have done the same but idk :P ) 
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b") && server.hasArg("w") && server.hasArg("rotation") && server.hasArg("fadeint")) {
//    String message = "";
    for (uint8_t i = 0; i < server.args(); i++) {
//      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
      // gets the seperated argument vals as string
      string0 = server.arg(0);
      string1 = server.arg(1);
      string2 = server.arg(2);
      string3 = server.arg(3);
      string4 = server.arg(4);
      string5 = server.arg(5);
      // and converts them to int
      rgb[0] = server.arg(0).toInt();
      rgb[1] = server.arg(1).toInt();
      rgb[2] = server.arg(2).toInt();
      w = server.arg(3).toInt();
      rotation = server.arg(4).toInt();
      fade_interval = server.arg(5).toInt();
    }
    //further converting to get the proper values needed
    fade_interval *= 500;
    rgb_alpha = (float)w / 100;
    // request store of last used values to EEPROM
    store_last=true;
  }
  // if the second form is used instead
  // checks if saved was requested
  else if (server.hasArg("saveVals")) {
      store_data = true;
    }
  }
  // or load 
  if (server.hasArg("loadVals")) {
      get_data = true;
    }
  }
}


void loop() {
  get_store2EEPROM();
  server.handleClient();
  store2EEPROM();
  stepper();
  init_rgbfade();
  if (fade_interval != 0) {
    rgb_fade();
  }
  led_control();
}
