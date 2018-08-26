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
// IR led send at pin D3 (https://wiki.wemos.cc/products:d1_mini_shields:ir_controller_shield)
#define IR_LED D3

const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";
const char* client_id = "";
const char* sub_topic = "";
const char* state_topic = "";

WiFiClient espClient;
PubSubClient client(espClient);
IRToshibaAC toshibair(IR_LED);

void setup_wifi() {
  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password); 

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    ESP.wdtFeed();
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
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

void publishState() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  
  root["power"] = toshibair.getPower();
  root["temperature"] = toshibair.getTemp();
  root["fan"] = toshibair.getFan();

  int mode = toshibair.getMode();

  if(mode == TOSHIBA_AC_COOL) {
    root["mode"] = "COOL";
  } else if(mode == TOSHIBA_AC_HEAT) {
    root["mode"] = "HEAT";
  } else if(mode == TOSHIBA_AC_AUTO) {
    root["mode"] = "AUTO";
  }

  char output[200];
  root.printTo(output);

  client.publish(state_topic, output);

  Serial.println("Published: ");
  Serial.println(output);
 }

void handle_led() {
  static int led_state = 0;
  static int ttl = 16;

  if(led_state == 0) {
    digitalWrite(BUILTIN_LED, LOW);
    led_state = 1;
  } else {
    digitalWrite(BUILTIN_LED, HIGH);
    led_state = 0;
  }
}

void handle_subscription(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(payload);
  if (!root.success()) {
    Serial.println("JSON parsing failed");
    return;
  }

  bool power = root["power"];
  const char* ac_mode = root["mode"];
  long temp = root["temperature"];
  long fan = root["fan"];

  // handle payload
  if(power) {
    toshibair.on();
    if(strcmp(ac_mode, "COOL") == 0) {
      toshibair.setMode(TOSHIBA_AC_COOL);
    } else if(strcmp(ac_mode, "HEAT") == 0) {
      toshibair.setMode(TOSHIBA_AC_HEAT);
    } else if(strcmp(ac_mode, "AUTO") == 0) {
      toshibair.setMode(TOSHIBA_AC_AUTO);
    } else {
      Serial.println("Unknown AC mode");
      return;
    }
    
    toshibair.setFan(fan);
    toshibair.setTemp(temp);
  } else {
    toshibair.off();
  }

  printState();
  publishState();
  Serial.println("Sending IR command to A/C ...");
  
  toshibair.send();
  handle_led();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Attempt to connect
    if (client.connect(client_id)) {
      Serial.println("connected");
      if(client.subscribe(sub_topic)) {
        Serial.print("Subscribed to ");
        Serial.println(sub_topic);
      }
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
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
  ESP.wdtEnable(1000);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(handle_subscription);
  toshibair.begin();
  digitalWrite(BUILTIN_LED, HIGH);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  ESP.wdtFeed();
}
