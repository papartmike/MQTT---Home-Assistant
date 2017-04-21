/*  MQTT switch (relay control)
   
    - WiFi INITIALISAZION: as described embedded
    - MQTT stndard pubsubclient LIBRARY.
        - reconnect() connect to the broker and SUBSRIBE to topics
        - callback() recieve the messages this client is subscribed to
    - turnON/turnOFF function -  payload 0 and 1

    home assistant config:

    switch:
      platform: mqtt
      name: "Banco lavoro"
      state_topic: "office/marcodesk/solder/setOK"
      command_topic: "office/marcodesk/solder/set"
      payload_on: "1"
      payload_off: "0"
      optimistic: false
      qos: 1
      retain: true
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

/************ WIFI and MQTT INFORMATION ******************/
#define wifi_ssid "HassWiFi"
#define wifi_password "hassbian"

#define mqtt_server "10.0.0.2"
//#define mqtt_user "user MOSQUITTO"
//#define mqtt_password "password MOSQUITTO"

/****************************** MQTT TOPICS  ***************************************/
#define RelayCommandSub "office/marcodesk/solder/set"         //turn ON turn OFF
#define RelayStatusPub  "office/marcodesk/solder/setOK"       //and confirm

/****************************** INPUTS/OUTPUTS  ***************************************/
#define Relay 0  //define pin

WiFiClient espClient;              //initialise a wifi client
PubSubClient client(espClient);   //creates a partially initialised client instance

char msg[50];
int RelayState = 0;

/****************************** SETUP WIFI  ***************************************/
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
      // Once connected, publish an announcement...
      client.publish(RelayStatusPub, "0");
      // ... and resubscribe
      client.subscribe(RelayCommandSub);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    msg[i] = payload[i];
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(Relay, HIGH);   // Turn the LED on
    RelayState = 1;
    client.publish(RelayStatusPub,"1");
  }   
  if ((char)payload[0] == '0') {
    digitalWrite(Relay, LOW);   // Turn the LED on
    RelayState = 0;
    client.publish(RelayStatusPub,"0");
  }

}
void setup() {
  // put your setup code here, to run once:
  pinMode(Relay, OUTPUT);
  Serial.begin(9600);
  digitalWrite(Relay, LOW);
  setup_wifi();
  client.setServer(mqtt_server, 1883);  //client is now ready for use
  client.setCallback(callback);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (!client.connected()) {
    reconnect();
  }
  client.loop();  
}
