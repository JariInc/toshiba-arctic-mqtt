/*
 * Based on work of 
 *   https://github.com/markszabo/IRremoteESP8266/blob/master/examples/TurnOnToshibaAC/TurnOnToshibaAC.ino
 *   https://github.com/knolleary/pubsubclient/blob/master/examples/mqtt_esp8266/mqtt_esp8266.ino
 *   https://arduinojson.org/v5/example/parser/
 */

// Bundled ESP8266WiFi headers
#include <ESP8266WiFi.h>

// PubSubClient https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>

// IRremoteESP8266 https://github.com/markszabo/IRremoteESP8266
#include <IRsend.h>
#include <ir_Toshiba.h>

// ArduinoJson https://arduinojson.org
#include <ArduinoJson.h>

// Configuration
// IR led at pin D2
#define IR_LED D2 
// Comment to disable serial monitor
#define DEBUG

const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";
const char* client_id = "";
const char* sub_topic = "";

WiFiClient espClient;
PubSubClient client(espClient);
IRToshibaAC toshibair(IR_LED);
DynamicJsonBuffer jsonBuffer;

void setup_wifi() {
  delay(10);

  #ifdef DEBUG
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  #endif
  
  WiFi.begin(ssid, password); 

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    #ifdef DEBUG
    Serial.print(".");
    #endif
  }

  #ifdef DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  #endif
}

void printState() {
  // Display the settings.
  Serial.println("Toshiba A/C remote is in the following state:");
  Serial.printf("  Power: %d,  Mode: %d, Temp: %dC, Fan Speed: %d\n",
                toshibair.getPower(), toshibair.getMode(), toshibair.getTemp(),
                toshibair.getFan());
  // Display the encoded IR sequence.
  unsigned char* ir_code = toshibair.getRaw();
  Serial.print("IR Code: 0x");
  for (uint8_t i = 0; i < TOSHIBA_AC_STATE_LENGTH; i++)
    Serial.printf("%02X", ir_code[i]);
  Serial.println();
}

void handle_subscription(char* topic, byte* payload, unsigned int length) {
  #ifdef DEBUG
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  #endif
  
  JsonObject& root = jsonBuffer.parseObject(payload);
  if (!root.success()) {
    #ifdef DEBUG
    Serial.println("JSON parsing failed");
    #endif
    return;
  }

  bool power = root["power"];
  const char* ac_mode = root["mode"];
  long temp = root["temperature"];
  long fan = root["fan"];
  
  // handle payload
  if(power) {
    toshibair.on();
  } else {
    toshibair.off();
  }

  if(strcmp(ac_mode, "COOL") == 0) {
    toshibair.setMode(TOSHIBA_AC_COOL);
  } else if(strcmp(ac_mode, "HEAT") == 0) {
    toshibair.setMode(TOSHIBA_AC_HEAT);
  } else {
    #ifdef DEBUG
    Serial.println("Unknown AC mode");
    #endif
    return;
  }
  
  toshibair.setFan(fan);
  toshibair.setTemp(temp);

  #ifdef DEBUG
  printState();
  Serial.println("Sending IR command to A/C ...");
  #endif
  
  toshibair.send();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    #ifdef DEBUG
    Serial.print("Attempting MQTT connection...");
    #endif
    
    // Attempt to connect
    if (client.connect(client_id)) {
      #ifdef DEBUG
      Serial.println("connected");
      #endif
      if(client.subscribe(sub_topic)) {
        #ifdef DEBUG
        Serial.print("Subscribed to ");
        Serial.println(sub_topic);
        #endif
      }
    } else {
      #ifdef DEBUG
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      #endif
      
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(handle_subscription);
  toshibair.begin();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
