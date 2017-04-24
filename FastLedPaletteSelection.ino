/*  MQTT FAST LED LIGHT CONTROLLED VIA HOME ASSISTANT with ESP8266:
    - WiFi INITIALISAZION: as described embedded
    - MQTT stndard pubsubclient LIBRARY.
        - reconnect() connect to the broker and subscribe to topics
        - callback() recieve the messages this client is subscribed to
    - turnON/turnOFF function - standard payload "ON" "OFF"
    - brightness control (no need confirmation for this
      within HA.only if there is an external control for that)
    - Palette selection (STATIC) through HA
    - RGB palette allows to control colors
    - Display the value of sensor_battery1 on the led strip


    HOME ASSISTANT LIGHT (RGB)

light:
- platform: mqtt
  name: 'Totem'
  command_topic: 'demo/totem/setpower'
  state_topic: 'demo/totem/setpowerOK'
  brightness_command_topic: 'demo/totem/setbrightness'
  brightness_state_topic: 'demo/totem/setbrightnessOK'
  rgb_command_topic: 'demo/totem/setcolor'
  rgb_state_topic: 'demo/totem/setcolorOK'
  #brightness_scale: 100
  optimistic: false
  retain: true

input_select:
  palette_select:
    name: Select Palette
    options:
     - "RGB"
     - "Semaforo"
     - "Rainbow"
     - "Clouds"
     - "YelRosePurp"
     - "Fire"
     - "Drago Verde"
     - "Trove"
     - "Peppermint"
    initial: "Semaforo"

automation:

- alias: "Select Palette"
  initial_state: True
  hide_entity: False
  trigger:
    - platform: state
      entity_id: input_select.palette_select
  action:
    - service: mqtt.publish
      data_template:
        topic: "demo/totem/selectpalette"
        payload: '{{ trigger.to_state.state | string }}'

- alias: "Bar Height"
  initial_state: True
  hide_entity: False
  trigger:
    - platform: state
      entity_id: sensor.battery_1
  action:
    - service: mqtt.publish
      data_template:
        topic: "demo/totem/battery1"
        payload_template: "{{ trigger.to_state.state | string }}"

*/

#define FASTLED_ALLOW_INTERRUPTS 0 //FOUNDAMENTAL TO AVOID GLITCHES with ESP!!!
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>


/************ FASTLED STUFF ******************/
#define NUM_LEDS 25
#define DATA_PIN 7
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define UPDATES_PER_SECOND 20


/************ WIFI and MQTT INFORMATION ******************/
#define wifi_ssid "HassWiFi"
#define wifi_password "hassbian"

#define mqtt_server "10.0.0.2"
//#define mqtt_user "user MOSQUITTO"
//#define mqtt_password "password MOSQUITTO"


/****************************** MQTT TOPICS  ***************************************/
#define SetpowerSub "demo/totem/setpower"        //turn ON turn OFF
#define SetpowerPub "demo/totem/setpowerOK"      //and confirm

#define SetbrightnessSub "demo/totem/setbrightness"    //set brightness
#define SetbrightnessPub "demo/totem/setbrightnessOK"  //and confirm (HA mqtt don't need confirmation, but i loses the data if turned OFF)

#define SetcolorSub "demo/totem/setcolor"
#define SetcolorPub "demo/totem/setcolorOK"

#define Battery1Sub "demo/totem/battery1"
#define SetPaletteSub "demo/totem/selectpalette"


char msg_buff[100]; // to store the message payload format for publishing

/************************* WI-FI (variables) **************************/
WiFiClient espClient;              //initialise a wifi client
PubSubClient client(espClient);   //creates a partially initialised client instance


/************ MORE FASTLED STUFF (variables)******************/


String setPower = "OFF";          //string recieved values
String setBrightness = "128";
String setColor = "255,241,224";
String setPalette = "RGB";
int brightness = 128;

int Rcolor = 255;
int Gcolor = 241;
int Bcolor = 224; //warm white

float batteryValue = 0.0;
int batteryLevel = 0;
int numLedsToLight;

CRGB leds[NUM_LEDS];
CRGBPalette16 LaPalette; //a type palette 16bit

float MqttValue;
//extern CRGBPalette16 myRainbowPalette_p;
extern const TProgmemPalette16 myTrafficLightsPalette_p PROGMEM;
extern const TProgmemPalette16 myClouds_p PROGMEM;

/************ SETUP WIFI CONNECT and PRINT IP SERIAL ******************/
void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());   
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  int i = 0;
  //turn on turn off command and publish
  if (String(p_topic) == SetpowerSub) {
    for (i = 0; i < p_length; i++) {
      msg_buff[i] = p_payload[i];
    }
    msg_buff[p_length] = '\0';
    setPower = String(msg_buff);
    Serial.println("Set Power: " + setPower);
    if (setPower == "OFF") {
      client.publish(SetpowerPub, "OFF");
    }

    if (setPower == "ON") {
      client.publish(SetpowerPub, "ON");
    }
  }

  if (String(p_topic) == SetbrightnessSub) {
    for (i = 0; i < p_length; i++) {
      msg_buff[i] = p_payload[i];
    }
    msg_buff[i] = '\0';
    setBrightness = String(msg_buff);
    Serial.println("Set Brightness: " + setBrightness);
    brightness = setBrightness.toInt();
    client.publish(SetbrightnessPub, msg_buff);
    //setPower = "ON";                            //logically correct but useless.
    //client.publish(SetpowerPub, "ON");          //HA publishes "ON" when controlling brightness
  }
  if (String(p_topic) == SetcolorSub) {
    for (i = 0; i < p_length; i++) {
      msg_buff[i] = p_payload[i];
      Serial.print(msg_buff[i]);
    }
    msg_buff[i] = '\0';
    setColor = String(msg_buff);
    Serial.println("Set color: " + setColor);
    Rcolor = setColor.substring(0, setColor.indexOf(',')).toInt();
    Gcolor = setColor.substring(setColor.indexOf(',') + 1, setColor.lastIndexOf(',')).toInt();
    Bcolor = setColor.substring(setColor.lastIndexOf(',') + 1).toInt();
    client.publish(SetcolorPub, msg_buff);
  }

  if (String(p_topic) == Battery1Sub) {     //read battery values
    p_payload[p_length] = '\0';  //terminate the string with NULL
    String message_buffer = String((char*)p_payload);
    batteryValue = message_buffer.toFloat();
    batteryLevel = (int)batteryValue;
    Serial.println("Message arrived [");
    Serial.print(batteryValue);
    Serial.print("] ");
    Serial.println();
  }

  if (String(p_topic) == SetPaletteSub) {
    for (i = 0; i < p_length; i++) {
      msg_buff[i] = p_payload[i];
    }
    msg_buff[i] = '\0';
    setPalette = String(msg_buff);
    if (setPalette == "RGB") {
      Serial.println("RGB mode active  -  pickup your color");
    }
    else {
      Serial.println("Set Palette: " + setPalette);
      AssignPalette(setPalette);
    }
    client.publish(SetpowerPub, "ON");

  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      if (setPower == "ON")
        client.publish(SetpowerPub, "ON"); // Once connected, publish state
      else
        client.publish(SetpowerPub, "OFF");
      // ... and resubscribe
      client.subscribe(SetpowerSub);
      client.subscribe(SetbrightnessSub);
      client.subscribe(SetcolorSub);
      client.subscribe(SetPaletteSub);
      client.subscribe(Battery1Sub);


    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void setup() {
  Serial.begin(9600);    //for debugging
  setup_wifi();
  client.setServer(mqtt_server, 1883);  //client is now ready for use
  client.setCallback(callback);

  delay(1000); //power-up safety delay
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 1000); //as the function says.....
  FastLED.setBrightness( brightness );
  FastLED.setDither(0);  //quando dimmera basso (i.e. 25% di 46 di brightness, il risultato è 11,5)
  //                       //senza questo comando fa un dithering tra 11 e 12 ---> reference:  https://github.com/FastLED/FastLED/wiki/FastLED-Temporal-Dithering

  
  LaPalette = myTrafficLightsPalette_p; //single color mapping HERE
  client.publish(SetcolorPub, "255,241,224");
  batteryLevel=100;
}

void loop() {

  
  numLedsToLight = map(batteryLevel, 0, 100, 0, NUM_LEDS);

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (setPower == "OFF") {
    for ( int i = 0; i < NUM_LEDS; i++)
      leds[i].fadeToBlackBy( 16 );   //FADE OFF LEDS

  }

  else if (setPalette != "RGB") {
    FastLED.clear();
    for (int i = 0; i < numLedsToLight; i++)
      leds[i] = ColorFromPalette( LaPalette, i * (255 / (NUM_LEDS - 1)), 255, LINEARBLEND);
  }
  else if (setPalette == "RGB") {
    fill_solid(leds, NUM_LEDS, CRGB(Rcolor, Gcolor, Bcolor));
    
  }

  FastLED.setBrightness(brightness);
  FastLED.show();
}



//yellow rose and purple
DEFINE_GRADIENT_PALETTE( bhw1_01_gp ) {
  0, 227, 101,  3,
  117, 194, 18, 19,
  255,  92,  8, 192
};

// azzurro verde giallino verde azzurro //
DEFINE_GRADIENT_PALETTE( bhw2_turq_gp ) {
  0,   1, 33, 95,
  38,   1, 107, 37,
  76,  42, 255, 45,
  127, 255, 255, 45,
  178,  42, 255, 45,
  216,   1, 107, 37,
  255,   1, 33, 95
};

// fire with some rose
DEFINE_GRADIENT_PALETTE( bhw3_02_gp ) {
  0, 121,  1,  1,    //red
  63, 255, 57,  1,   //orange
  112, 255, 2, 3,  //red
  145, 255, 20, 5,  //red
  188, 244, 146,  3, //yellow
  255, 115, 14,  1   //redish
};

DEFINE_GRADIENT_PALETTE( trove_gp ) {
  0,  12, 23, 11,
  12,   8, 52, 27,
  25,  32, 142, 64,
  38,  55, 68, 30,
  51, 190, 135, 45,
  63, 201, 175, 59,
  76, 186, 80, 20,
  89, 220, 79, 32,
  101, 184, 33, 14,
  114, 137, 16, 15,
  127, 118, 20, 27,
  140,  79, 16, 35,
  153,  67,  8, 26,
  165,  22,  9, 42,
  178,  11,  3, 34,
  191,  58, 31, 109,
  204, 186, 49, 83,
  216, 182, 25, 55,
  229,  39, 90, 100,
  242,  15, 81, 132,
  255,  68, 135, 52
};

// Gradient palette "Sunrise_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ggr/tn/Sunrise.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 52 bytes of program space.

DEFINE_GRADIENT_PALETTE( Sunrise_gp ) {
    0, 255,255,255,
   25, 237,246,199,
   51, 222,237,151,
   96, 237,139, 54,
  124, 255, 69,  9,
  128, 139, 21,  5,
  134,  61,  1,  2,
  139,  29,  2,  1,
  143,   9,  4,  1,
  155,  12, 16, 29,
  177,  15, 39,145,
  215,  65,118,197,
  255, 167,246,255};



//LaPalette = myRainbowPalette_p; incase assign palette before fire the leds

void AssignPalette(String p_Palette) {

  //if (p_Palette == "RGB") SetStaticColor();
  if (p_Palette == "Semaforo") LaPalette = myTrafficLightsPalette_p;
  else if (p_Palette == "Rainbow") LaPalette = RainbowColors_p;
  else if (p_Palette == "Clouds") LaPalette = myClouds_p;
  else if (p_Palette == "YelRosePurp") LaPalette = bhw1_01_gp;
  else if (p_Palette == "Fire") LaPalette = bhw3_02_gp;
  else if (p_Palette == "Drago Verde") LaPalette = bhw2_turq_gp;
  else if (p_Palette == "Trove") LaPalette = trove_gp;
  else if (p_Palette == "Peppermint") LaPalette = Sunrise_gp;

}


const TProgmemPalette16 myTrafficLightsPalette_p PROGMEM =
{
  CRGB::Red,
  CRGB::Red,
  CRGB::Red,
  CRGB::Red,

  CRGB::OrangeRed,
  CRGB::OrangeRed,
  CRGB::OrangeRed,
  CRGB::Orange,

  CRGB::Orange,
  CRGB::Yellow,
  CRGB::LawnGreen,
  CRGB::LawnGreen,

  CRGB::Green,
  CRGB::Green,
  CRGB::Green,
  CRGB::Green

};

const TProgmemPalette16 myClouds_p PROGMEM =
{
  CRGB::Aqua,
  CRGB::Aqua,
  CRGB::Gray,
  CRGB::Cyan,

  CRGB::Gray,
  CRGB::Gray,
  CRGB::CornflowerBlue,
  CRGB::CornflowerBlue,

  CRGB::DeepSkyBlue,
  CRGB::DeepSkyBlue,
  CRGB::Gray,
  CRGB::Gray,

  CRGB::Gray,
  CRGB::DarkCyan,
  CRGB::DarkCyan,
  CRGB::DarkCyan,

};
