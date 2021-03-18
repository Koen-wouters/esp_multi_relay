/*
Some of the below is borrowed, and most is heavily inspired by people much smarter than me.
I am not able to give credits to whome it deserves due to all the copy paste and changes, 
so if you recognize part of your efforts please let me know and I will include it.
*/

/* 
This code allows you to make full use on your home network of the popular and cheap multirelay ESP01 driven boards like this one : 
https://www.banggood.com/DC12V-ESP8266-Four-Channel-Wifi-Relay-IOT-Smart-Home-Phone-APP-Remote-Control-Switch-p-1317255.html
The code is optimized to work with MQTT, but the logic can be easily adapted to any other way to trigger the relays.
It's far from perfect or finished, but it works...
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>

#define fDebug true

// Relay codes. The boards with 2 relays use the same codes (1 and 2 in that case)
/*
  Open  1st relay: A0 01 01 A2
  Open  2nd relay: A0 02 01 A3
  Open  3rd relay: A0 03 01 A4
  Open  4th relay: A0 04 01 A5

  Close 1st relay: A0 01 00 A1
  Close 2nd relay: A0 02 00 A2
  Close 3rd relay: A0 03 00 A3
  Close 4th relay: A0 04 00 A4
*/

byte relON[4][4] = {{0xA0, 0x01, 0x01, 0xA2}, {0xA0, 0x02, 0x01, 0xA3}, {0xA0, 0x03, 0x01, 0xA4}, {0xA0, 0x04, 0x01, 0xA5}}; // Hex command to send to serial for open relay
byte relOFF[4][4] = {{0xA0, 0x01, 0x00, 0xA1}, {0xA0, 0x02, 0x00, 0xA2}, {0xA0, 0x03, 0x00, 0xA3}, {0xA0, 0x04, 0x00, 0xA4}}; // Hex command to send to serial for close relay

boolean relayState[4] = {false, false, false, false};
boolean relayLastState[4] = {false, false, false, false};

const int NUMBER_OF_RELAYS = 4; // 4 or 2 in case of smaller board

#define mqtt_server "192.168.xx.xxx"  // use your own 
const PROGMEM char*     MQTT_CLIENT_ID            = "esp01_relay";  // change with whatever you want, if you want
const PROGMEM char*     MQTT_USER                 = "user";         // change with MQTT user
const PROGMEM char*     MQTT_PASSWORD             = "password";     // change with your MQTT password

const int MAX_TOPIC_LENGTH = 7; // max length of topic +1

// Topics for MQTT. Here it's simple relay1 to relay4, but you can use your imagination

char MQTT_TOPIC [NUMBER_OF_RELAYS] [MAX_TOPIC_LENGTH] = {
  { "relay1" },
  { "relay2" },
  { "relay3" },
  { "relay4" }
};

const char*             MQTT_RELAY_STATE_TOPIC    = "something/for/mqtt/";        // Use some descriptive topic like relays/livingroom/main
const char*             MQTT_RELAY_COMMAND_TOPIC  = "something_else/for/mqtt/";
const char*             RELAY_ON                  = "ON";
const char*             RELAY_OFF                 = "OFF";

WiFiClient esp01_relay; // whatever makes sense, just keep it unique if you have more than one 
PubSubClient client(esp01_relay);
int setIP = 160;  // If you want to set a fixed ip, can be anything supported by your router, in this case it will be 160

const char* ssid     = "your_network_name";
const char* password = "your_network_password";

IPAddress local_IP(192, 168, 68, setIP); // Put the static IP addres you want to assign here, change with your own IP!
IPAddress gateway(192, 168, 68, 1); // Put your router gateway here
IPAddress subnet(255, 255, 255, 0);

unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

// Listen for incoming messages
void mqttCallback(char* topic, byte* message, unsigned int length) {

  // Incoming message to string (payload)
  String payload;
  for (int i = 0; i < length; i++) {
    //Serial.print((char)message[i]);
    payload += (char)message[i];
  }

  for (int i = 0; i < NUMBER_OF_RELAYS; i++) {
    String CT = MQTT_RELAY_COMMAND_TOPIC + String(MQTT_TOPIC[i]);

    if (String(CT).equals(topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(RELAY_ON))) {
        if (relayState[i] != true) {
          if (fDebug) {
          Serial.printf("Relay %u Turn ON message received \n", i);
        }
          relayState[i] = true;
          setRelayState();
          //publishRelayState();
        }
      } else if (payload.equals(String(RELAY_OFF))) {
        if (fDebug) {
          Serial.printf("Relay %u Turn OFF message received \n", i);
        }
        relayState[i] = false;
        setRelayState();
        //publishRelayState();
      }
    }
    
  }
  
}

// Reconnect when bad network
void mqttReconnect() {
  // Loop until we're reconnected
  int counter = 0;
  while (!client.connected()) {
    if (counter==5){
      ESP.restart();
    }
    counter+=1;
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
   
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
      mqttSubscribe();
      publishRelayState();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  
}

// Subribe to MQTT Topics of interest
void mqttSubscribe() {
  for (int i = 0; i < NUMBER_OF_RELAYS; i++) {
    String CT = MQTT_RELAY_COMMAND_TOPIC + String(MQTT_TOPIC[i]);
    client.subscribe(CT.c_str());
    }
}

void startWifi(){
  // Set device as a both Wi-Fi Station and access point
  WiFi.mode(WIFI_AP_STA);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    Serial.println("");
    Serial.println("Setting as a Wi-Fi Station..");
  }

  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  Serial.println();
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  Serial.println();
  Serial.print("ESP Board MAC Address 2:  ");
  Serial.println(WiFi.softAPmacAddress());
  
}

void setRelayState(){
    for (int i = 0; i < NUMBER_OF_RELAYS; i++) {
    String CS = MQTT_RELAY_STATE_TOPIC + String(MQTT_TOPIC[i]);
    // Only publish when changed
    if (relayState[i] != relayLastState[i]){
        if (relayState[i] == HIGH) {
            // Here is where the magic is, we send the right command to the right relay. Here we put it on
            Serial.write(relON[i], sizeof(relON[i]));
            if (fDebug) {
              Serial.printf("\nOn Command send for relay : %u \n", i);
            }
            
          } else {
            // Here we turn it off
            Serial.write(relOFF[i], sizeof(relOFF[i]));
            if (fDebug) {
              Serial.printf("\nOff Command send for relay : %u \n", i);
            }
            
          }
          
        publishRelayState();
      }
    }
    
  }

void publishRelayState(){

  for (int i = 0; i < NUMBER_OF_RELAYS; i++) {
    String CS = MQTT_RELAY_STATE_TOPIC + String(MQTT_TOPIC[i]);
        if (relayState[i] == HIGH) {
            if (fDebug) {
              Serial.printf("Relay %u State High Status send\n", i);
            }
            client.publish(CS.c_str(), RELAY_ON, true);
          } else {
            if (fDebug) {
              Serial.printf("Relay %u State Low Status send \n", i);
            }
            client.publish(CS.c_str(), RELAY_OFF, true);
          }
          relayLastState[i] = relayState[i];
          Serial.printf("Last State modified \n", i);
      }
    
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  startWifi();
  
  // MQTT Setup
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    
    // Check if we still are connected with the MQTT broker, otherwise reconnect
    // On start up this will run by default and subscribe to all topics we care about
    if (!client.connected()){
      mqttReconnect();
    }    
    lastTime = millis();
  }
  client.loop();
}
