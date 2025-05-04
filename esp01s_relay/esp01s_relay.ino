/*
 * Mouse trap, based on mqtt mesage
 * Mqtt client will subcribe to anhtuan/mousetrap/event topic 
 * If any message received,  trigger will be togged ON on 30 sec
 */
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266TimerInterrupt.h>

#define RELAY_GPIO  0  // relay connected to  GPIO0
#define RELAY_ON    true
#define RELAY_OFF   false

// Variables for PubSubClient
const char* ssid = "AnhTuan_VNPT";
const char* password = "Abc@13579";
const char* mqtt_host = "mqtt.smartsolar.io.vn";
const int mqtt_port = 1883;
const char* mqtt_user = "tuanna";
const char* mqtt_pwd = "Abc@13579";
const char* topic_subscribe = "anhtuan/mousetrap/event";
char mqtt_client_name[30];

WiFiClient wifiClient;
PubSubClient _mqttClient(wifiClient);

// Definitions for ITimer:
#define TIMER_INTERVAL_MS WDTO_4S
#define STATUS_TIMER_INTERVAL_MS 30000
// Init ESP8266 timer 1
ESP8266Timer ITimer;

// For application domains:
int _relayStatus = 0;


/*
 *------------------------------------------------------------------
 * TimerHandler function - Called on ITimer(Timer1) ticked
 *------------------------------------------------------------------
 */
static int g_timerCounter = 0;
void IRAM_ATTR TimerHandler() {
  if (g_timerCounter++ >= STATUS_TIMER_INTERVAL_MS / TIMER_INTERVAL_MS) {
    g_timerCounter = 0;
    // Check every 4x30=120 sec
    // Other code:
    // Auto toggle off relay after 30s:
    if(_relayStatus){
      Serial.printf("[INFO][AT_Timer] Auto turn OFF relay to keep safe.\n");
      toggleRelay(RELAY_OFF);
    }
  }
}


void setupTimer() {
  // Interval in microsecs
  if (ITimer.attachInterruptInterval(TIMER_INTERVAL_MS * 1000, TimerHandler)) {
    Serial.printf("[Info][AT_Timer] Started ITimer for %d ms\n", TIMER_INTERVAL_MS);
  } else
    Serial.println("[Info][AT_Timer] Can't set ITimer correctly. Select another freq. or interval");
}

/*
 *------------------------------------------------------------------
 * mqtt_callback function - Called on a new mqtt message received
 *------------------------------------------------------------------
 */
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  // Blink Led:
  blinkLed();

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch relay:
  if ((char)payload[0] == '1') {
    toggleRelay(RELAY_ON);
  } else {
    toggleRelay(RELAY_OFF);
  }
}

/*
 *------------------------------------------------------------------
 * Wifi functions
 *------------------------------------------------------------------
 */
void setupWifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
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

/*
 *------------------------------------------------------------------
 * Mqtt / PubSubClient functions
 *------------------------------------------------------------------
 */
void connfigMqttClient() {
  // Format client name:
  sprintf(mqtt_client_name, "ESP8266_MouseTrap_%04X", random(0xffff));
  // Setup mqtt client
  _mqttClient.setServer(mqtt_host, mqtt_port);
  _mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
  _mqttClient.setCallback(mqtt_callback);
}

int reCount = 0;
bool connectToServer() {
  Serial.printf("[INFO][AT_Mqtt] Starting conect to Mqtt broker");
  // Attempt to connect
  if (_mqttClient.connect(mqtt_client_name, mqtt_user, mqtt_pwd)) {
    reCount = 0;
    Serial.println("[INFO][AT_Mqtt] Mqtt broker connected, client name: " + String(mqtt_client_name));
    //Subscribe a control topic: anhtuan/mousetrap/event
    _mqttClient.subscribe(topic_subscribe);
    Serial.printf("[INFO][AT_Mqtt] Mqtt subscribed topic: %s\n", topic_subscribe);
    Serial.println();
  } else {
    Serial.printf("[Error][AT_Mqtt] Mqtt connect failed (%d), rc=%d\n", reCount + 1, _mqttClient.state());
    if (reCount++ > 5) {
      Serial.printf("[VERBOSE][AT_Mqtt] MqttClient can not connect to broker for a long time. ESP now restarted.\n");
      ESP.restart();
    }
  }
  return _mqttClient.connected();
}

/*
 *------------------------------------------------------------------
 * Other utilities functions
 *------------------------------------------------------------------
 */
void blinkLed() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);  // wait for a second
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
  }
}

void toggleRelay(bool relayState) {
  if (relayState) {
    _relayStatus = 1;
    digitalWrite(RELAY_GPIO, LOW);
  } else {
    _relayStatus = 0;
    digitalWrite(RELAY_GPIO, HIGH);
  }
}

/*
 *------------------------------------------------------------------
 * Setup and loop functions
 *------------------------------------------------------------------
 */
void setup() {
  // Randomize seed
  randomSeed(micros());

  Serial.begin(9600);
  delay(10);
  // Initialize the BUILTIN_LED pin as an output GPIO
  pinMode(BUILTIN_LED, OUTPUT);
  // Initialize the RELAY_GPIO pin as an output GPIO
  pinMode(RELAY_GPIO, OUTPUT);
  digitalWrite(RELAY_GPIO, HIGH);

  // Connect to WiFi network
  setupWifi();
  // Config Mqtt client
  connfigMqttClient();
  // Connect to Mqtt broker
  connectToServer();

  // Setup Timer:
  setupTimer();
}

long _lastReconnectAttempt;
void loop() {
  if (!_mqttClient.connected()) {
    long now = millis();
    if (now - _lastReconnectAttempt > 5000) {
      _lastReconnectAttempt = now;
      // Attempt to reconnect
      if (WiFi.status() == WL_CONNECTED) {
        if (connectToServer())
          _lastReconnectAttempt = 0;
      } else {
        Serial.println("[Info][AT_Mqtt] Mqtt broker not connected due to wifi connection is lost");
      }
    }
  } else {
    // Client connected
    _mqttClient.loop();
  }
}
