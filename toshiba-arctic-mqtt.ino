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
#define IR_LED D2 // IR led at pin D2

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
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
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

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

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
  } else {
    toshibair.off();
  }

  Serial.print("AC mode ");
  Serial.println(ac_mode);

  if(strcmp(ac_mode, "COOL") == 0) {
    Serial.println("set COOL");
    toshibair.setMode(TOSHIBA_AC_COOL);
  } else if(strcmp(ac_mode, "HEAT") == 0) {
    Serial.println("set HEAT");
    toshibair.setMode(TOSHIBA_AC_HEAT);
  } else {
    Serial.println("Unknown AC mode");
    return;
  }
  
  toshibair.setFan(fan);
  toshibair.setTemp(temp);
  
  printState();
  Serial.println("Sending IR command to A/C ...");
  toshibair.send();
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
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  toshibair.begin();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
