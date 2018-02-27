#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <ESP8266httpUpdate.h>
#include <NewPing.h>
#include <Grove_LED_Bar.h>

#define ONE_WIRE_BUS 4
#define TRIGGER_PIN  12  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN     13  // Arduino pin tied to echo pin on the ultrasonic sensor.

const char* ssid = "*";
const char* password = "*";
const char* mqtt_server = "i-alice.ru";
const char* mqtt_alive_topic = "/borishome/cesspool/ultrasonic/alive";
const char* mqtt_subscribe_topic = "/borishome/cesspool/ultrasonic/+";
const char* mqtt_temp_topic = "/borishome/cesspool/ultrasonic/temp";
const char* mqtt_dist_topic = "/borishome/cesspool/ultrasonic/distance";
const char* mqtt_bar_topic = "/borishome/cesspool/ultrasonic/barlevel";
const char* update_url = "http://i-alice.ru/ota/index.php";
const char* update_class = "cesspool_sensor-0.0.1";

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;
WiFiClient espClient;
PubSubClient client(espClient);
NewPing sonar(TRIGGER_PIN, ECHO_PIN);
Grove_LED_Bar bar(14, 16);
long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  Serial.begin(115200);
  setup_wifi();
  updateProc();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");
  sensors.setResolution(insideThermometer, 9);
}

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
  Serial.println("STA-MAC: ");
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

void callback(char* topic, byte* payload, unsigned int length) {
  char buf[10];
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    buf[i]=(char)payload[i];
    Serial.print((char)payload[i]);
  }
  buf[length]=0;
  Serial.println();
  if (String(topic) == String(mqtt_bar_topic)) {
    //String(payload).toCharArray(buf, sizeof(buf));
    Serial.println(buf);
    bar.setLevel(atof(buf));
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("CesspoolSensor", mqtt_alive_topic, 0, 0, "0")) {
      Serial.println("connected");
      client.subscribe(mqtt_subscribe_topic);
      client.publish(mqtt_alive_topic, "1");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void updateProc() {
  if (WiFi.status() == WL_CONNECTED) {
    t_httpUpdate_return ret = ESPhttpUpdate.update(update_url, update_class);
    switch(ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            Serial.println("");
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[update] Update no Update.");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("[update] Update ok."); // may not called we reboot the ESP
            break;
    }
  }
}

void loop() {
  char buf[10];
  float temp;
  long distance;
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 15000) {
    lastMsg = now;
    Serial.print("Requesting temperatures...");
    sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.println("DONE");
    Serial.print("Temperature for the device 1 (index 0) is: ");
    temp = sensors.getTempCByIndex(0);
    if ((temp > -50) && (temp < 50)) {
      Serial.println(dtostrf(temp, 3, 2, buf));
      client.publish(mqtt_temp_topic, dtostrf(sensors.getTempCByIndex(0), 3, 2, buf));
    }
    distance = sonar.ping_cm();
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
    client.publish(mqtt_dist_topic, dtostrf(distance, 3, 2, buf));
    //bar.setLevel(distance/50);
  }
}
