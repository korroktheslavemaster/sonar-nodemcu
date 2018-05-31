/*
 Basic ESP8266 MQTT example

 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP8266 board/library.

 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off

 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.

 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
/*
********************************************
14CORE ULTRASONIC DISTANCE SENSOR CODE TEST
********************************************
*/
#define TRIGGER D1
#define ECHO    D2
#define MOTOR D6
#define MOTOR_LED D5
// 1 is off, 0 is on for some reason
#define ANNOYING_LED D4


const float TANK_FULL_DISTANCE_CM = 14.83; // distance at which 'aap ki paani ki tanki full hai'
const float TANK_DEPTH_CM = 109.982; // 43.3 inches

// Update these with values suitable for your network.

const char* ssid = "Anurag 2";
const char* password = "pavilion";
//const char* mqtt_server = "test.mosquitto.org";
const char* mqtt_server = "mqtt.thingspeak.com";
const int mqtt_port = 1883;
//const char* mqtt_server = "m10.cloudmqtt.com";
const char* mqtt_username = "a-unique-username";
const char* mqtt_password = "ZTO2255CUWOK0RIU";
const char* STATUS_TOPIC = "channels/507497/publish/4M5ZYM7CDET6C3J5";
const char* MOTOR_TOPIC = "channels/507509/subscribe/fields/field1/TPBLNPPKS1DQFTBR";

bool isManuallySwitchedOnFlag = false;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
double distances[5];
int n_distances = sizeof(distances)/sizeof(distances[0]);
int distanceIdx = 0;
char msg[200];
int value = 0;

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
  
  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // setup time
  // use GMT only
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");
}

struct Status {
  double distance, percentage; // calc percentage here only
  bool motor;
  time_t timestamp;
};

double getPercentage(double distance) {
  if (distance < TANK_FULL_DISTANCE_CM) return 1;
  else
    return (
      (TANK_DEPTH_CM - (distance - TANK_FULL_DISTANCE_CM)) / TANK_DEPTH_CM
    );
}

struct Status getStatus() {
  struct Status s1;
  s1.distance = getDistancesMedian(distances, n_distances);
//  s1.distance = getDistance();
  s1.percentage = getPercentage(s1.distance);
  s1.motor = digitalRead(MOTOR);
  s1.timestamp = time(nullptr);
  return s1;
}

void sendStatus(struct Status s1) {
//  snprintf(msg, 199, "{\"distance\": %.3lf, \"percentage\": %.5lf, \"motor\": %d, \"timestamp\": %ld }", s1.distance, s1.percentage, s1.motor, s1.timestamp);
  // for thingspeak:
  snprintf(msg, 199, "field1=%.3lf&field2=%.3lf&field3=%d&field4=%ld", s1.distance, s1.percentage * 100, s1.motor, s1.timestamp);
  Serial.println(msg);
  client.publish(STATUS_TOPIC, msg);
}

void writeMotor(bool val, bool manual = false);

void writeMotor(bool val, bool manual) {
  // digitalWrite both motor and led pin
  if (val && manual) {
    isManuallySwitchedOnFlag = true;
  } else {
    isManuallySwitchedOnFlag = false;
  }
  digitalWrite(MOTOR, val);
  digitalWrite(MOTOR_LED, val);
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  if (strcmp(topic, MOTOR_TOPIC) == 0) {
    // switch motor
    if ((char)payload[0] == '1') {
      writeMotor(HIGH, true);
    } else {
      writeMotor(LOW, true);
    }
    // send status
    delay(10);
    sendStatus(getStatus());
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
//      client.publish(OUT_TOPIC, "hello world");
      // ... and resubscribe
      client.subscribe(MOTOR_TOPIC);
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
  pinMode(MOTOR, OUTPUT);
  pinMode(MOTOR_LED, OUTPUT);
  pinMode(TRIGGER, OUTPUT);
  pinMode(ANNOYING_LED, OUTPUT);
  digitalWrite(ANNOYING_LED, HIGH);
  pinMode(ECHO, INPUT);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

double getDistance() {
  long duration;
  double distance;
  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  
  digitalWrite(TRIGGER, LOW);
  duration = pulseIn(ECHO, HIGH);
  distance = (duration/2.0) / 29.1;
  return distance;
//// DEBUGGING
//return 20.0;
}

double getDistancesMedian(double distances[], int n_distances) {
  // bubble sort
  bubble_sort(distances, n_distances);
  // return middle;
  return distances[n_distances/2];  
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;
    distances[distanceIdx++] = getDistance();
    if (distanceIdx % n_distances == 0) {
      distanceIdx = 0;
      // do the shit
      Status s1 = getStatus();
      doFillUp(s1);
      // get new value of motor now since it might have changed...
      s1.motor = digitalRead(MOTOR);
      sendStatus(s1);
    }
  }
//  if (now - lastMsg > 5000) {
//    lastMsg = now;
//    Status s1 = getStatus();
//    doFillUp(s1);
//    // get new value of motor now since it might have changed...
//    s1.motor = digitalRead(MOTOR);
//    sendStatus(s1);
//  }
}
