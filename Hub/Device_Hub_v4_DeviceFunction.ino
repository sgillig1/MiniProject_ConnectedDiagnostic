#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Button.h>
#include <PubSubClient.h>   //
#include "config.h" // include the configuration of credentials in the config.h file

//OLED
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//OLED
#define SCREEN_WIDTH 128 // OLED display width, in pixels
//#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// MPL 
#include <Adafruit_MPL115A2.h>
Adafruit_MPL115A2 mpl115a2;

// Servo
#if defined(ARDUINO_ARCH_ESP32)
  // ESP32Servo Library (https://github.com/madhephaestus/ESP32Servo)
  // installation: library manager -> search -> "ESP32Servo"
  #include <ESP32Servo.h>
#else
  #include <Servo.h>
#endif
// pin used to control the servo
#define SERVO_PIN 32
// create an instance of the servo class
Servo servo;
int angle = 0;

WiFiClient espClient;             //espClient
PubSubClient mqtt(espClient);     //tie PubSub (mqtt) client to WiFi client
char clientID[6]; //A MAC address is a 'truly' unique ID for each device, lets use that as our 'truly' unique user ID!!!

typedef struct { //here we create a new data type definition, a box to hold other data types
  const char* item1;
  float p1;
  const char* item2;
  float p2;
  const char* item3;
  float p3;
} imageIn;     //then we give our new data structure a name so we can use it in our code

imageIn imageInputDoc;

const int buttonPin = 14; // we'll use pin 14 on the ESP
const int buttonPin2 = 15; // we'll use pin 14 on the ESP
Button btn (buttonPin, INPUT_PULLUP);
Button btn2 (buttonPin2, INPUT_PULLUP);
unsigned long currentMillis, timerOne, deviceTimer, startTime;
unsigned long actuationTime = 10*1000; // Setup the time for the actuation steps
unsigned long detectionTime = 10*1000;
String zipCode;
float pressureKPA = 0, temperatureC = 0; 
int offAngle = 0;
int actuationAngle = 90;

int red_light_pin = 27;
int green_light_pin = 12;
int blue_light_pin = 33;
int led_off = 21;

// Below are the booleans to control the steps of the device
bool cameraConnect = false;
bool cameraFail = false;
bool setupTime = true;
bool startDevice = false;
bool capturePrompt = false;
bool captureResults = false;
bool captureReset = false;
bool locationAPI = false;
bool published = false;
bool publishDoctor = false;
bool finish = false;

int patientID = random(100000, 999999); // Create a patient ID
String call;
String realCall;
float confidenceCall;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  delay(10);
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));
  delay(2000);

   // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
   // Clear the buffer
  display.clearDisplay();
  // Draw a single pixel in white - just to make sure this thing works
  display.drawPixel(10, 10, SSD1306_WHITE);
  display.display();
  delay(2000);
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  // Display the connected diagnostics startup
  display.clearDisplay();
  display.setCursor(0, 0);  
  display.println("Welcome to Connected Diagnostics");
  display.println();
  display.display();
  delay(1000);
  display.clearDisplay();
  display.setCursor(0, 0);  
  display.println("System Check");
  display.println();
  display.display();
  // LED Setup
  display.print("LED: ");
  display.display();
  pinMode(red_light_pin, OUTPUT);
  pinMode(green_light_pin, OUTPUT);
  pinMode(blue_light_pin, OUTPUT);
  pinMode(led_off, OUTPUT);
  LEDTest();
  display.println("Functional");
  display.display();

  display.print("WiFi: ");
  display.display();
  connectWifi();
  display.println("Connected");
  display.print("MQTT: ");
  display.display();
  mqtt.setServer(mqtt_server, mqtt_port); // setup the MQTT object
  mqtt.setCallback(callback);
  display.println("Connected");
  display.display();

 
  // Setup MPL
  display.print("Temp: ");
  display.display();
  Serial.println("Getting barometric pressure ...");
  if (! mpl115a2.begin()) {
    Serial.println("Sensor not found! Check wiring");
    while (1);
  }
  getTemp();
  display.print(temperatureC, 0);
  display.print(" C");
  display.display();
  // Setup Servo
  servo.attach(SERVO_PIN);
  servo.write(offAngle);
  timerOne = millis();
  // Get Zipcode information
  getZipCode();
  display.print(" Zip: ");
  display.println(zipCode);
  display.display();

  // Check camera
  display.print("Camera: ");
  display.display();
  mqtt.loop();
  if (!mqtt.connected()) {
    reconnect();
  }
  publishInstruction("CameraCheck");
  Serial.print("Connecting to Camera");
  delay(1000);
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop(); //this keeps the mqtt connection 'active'
  if (!cameraConnect) {
    delay(10);
    return;
  }
  else if (setupTime){ // This allows for the first iteration of the loop to run the setup screen
    display.print("Connected");
    display.display();
    Serial.println();
    setupTime = false;
    delay(2000);
    display.clearDisplay();
    display.setCursor(0, 0);  
    display.println("Enter Cartridge and");
    display.println("Press Button ...");
    display.println();
    display.print("Patient ID:");
    display.println(patientID);
    display.display();
    delay(1000);
  }
  int action = btn.checkButtonAction();
  int action2 = btn2.checkButtonAction();
  // Wait for button click
  if (!startDevice and action == Button::CLICKED) {
    startTime = millis();
    startDevice = true;
  }
  if (action2 == Button::HELD_CLICKED) { // This will act as a reset for the system without need to reconnect
    setupTime = true;
    startDevice = false;
    capturePrompt = false;
    captureResults = false;
    captureReset = false;
    locationAPI = false;
    published = false;
    publishDoctor = false;
    finish = false;
    patientID = random(100000, 999999);
  }
  if (startDevice) {
    if (millis() - startTime < actuationTime) {runningDisplay();}
    //  This will actuate the servo after set time  
    if (millis() - startTime > actuationTime and millis() - startTime < actuationTime + detectionTime) {
      servo.write(actuationAngle);
      runningDisplay();
      display.println();
      display.println("Release for Detection");
      display.display();
    }
    // This will send the information to capture the photo
    if (millis() - startTime > actuationTime + detectionTime and !captureResults) {
      if (!capturePrompt) {
        publishInstruction("capture");
        capturePrompt = true;
      }
      if (captureReset) {
        publishInstruction("capture");
        captureReset = true;
      }
      runningDisplay();
      display.println();
      display.println("Detection in Progress");
      display.display();
    }
     if (captureResults) { // If receive the call, the analyse and display
        analyzeCall();
        displayImage();
        delay(1000);
        startDevice = false;
     }
  }
  if (!startDevice and captureResults and !finish) {
    if (!published) { // Publish results
      publishResults();
      published = true;
    }
    display.clearDisplay();
    display.setCursor(0, 0);  
    display.println("Publishing Results");
    display.println();
    display.display();
    if (publishDoctor){
      display.println("Results Published");
      display.display();
      delay(2000);
      display.clearDisplay();
      display.setCursor(0, 0);  
      display.println("Receiving Resources");
      display.println();
      display.display();
      delay(2000);
      display.println("Resources Attached");
      display.println();
      display.display();
      delay(2000);
      finish = true;
    }
  }

  if (finish) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Final Results"); 
    display.println();
    display.print(realCall);
    display.print(": ");
    display.println(imageInputDoc.p1);
    if (call == "Error") {
      display.print(call);
      display.print(": ");
      display.println(imageInputDoc.p1);
    }
    display.display();
    delay(10);
  }
  
}
// Connect to the wifi
void connectWifi() {
  Serial.print("Wifi Connecting");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(WiFi.macAddress());  //.macAddress() returns a byte array 6 bytes representing the MAC address
  String temp = WiFi.macAddress();
  temp.toCharArray(clientID, 6);
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(clientID, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe(feed1);  // Subscribe to the feeds.
      mqtt.subscribe(feed2);  // You could also do this in a single line by subscribing to all feeds  
      mqtt.subscribe(feed3);  // You could also do this in a single line by subscribing to all feeds        
    } else {                        //please change 'theTopic' to reflect your topic you are subscribing to
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
// Display the call for the test
void displayImage() {
  display.println();
  display.print(imageInputDoc.item1);
  display.print(": ");
  display.println(imageInputDoc.p1);
  if (call == "Error") {
    display.print(call);
    display.print(": ");
    display.println(imageInputDoc.p1);
  }
//  display.print(imageInputDoc.item2);
//  display.print(": ");
//  display.println(imageInputDoc.p2);
//  display.print(imageInputDoc.item3);
//  display.print(": ");
//  display.println(imageInputDoc.p3);
  display.display();
}

void runningDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);  
  display.println("Device Running");
  display.print("Time: ");
  long current = (millis()-startTime)/1000;
  display.print(current,1);
  display.println(" sec");
  display.display();
}
// Get the IP address for location
String getIP() {
  HTTPClient theClient;
  String ipAddress;

  theClient.begin("http://api.ipify.org/?format=json");
  int httpCode = theClient.GET();

  if (httpCode > 0) {
    if (httpCode == 200) {
      StaticJsonDocument<100> doc;
      String payload = theClient.getString();
      //   JsonObject& root = jsonBuffer.parse(payload);
      deserializeJson(doc, payload);
      ipAddress = doc["ip"].as<String>();

    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
      return "error";
    }
  }
  Serial.println(ipAddress);
  return ipAddress;
}
// Get the zipcode from IP Address
void getZipCode() {
  HTTPClient theClient;
  Serial.println("Making HTTP request");
  theClient.begin("http://api.ipstack.com/" + getIP() + "?access_key=" + locationKey); //return IP as .json object
// http://api.ipstack.com/97.113.62.196?access_key=...
  
  int httpCode = theClient.GET();

  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("Received HTTP payload.");
 //   alternatively use:  DynamicJsonDocument doc(1024); // specify JSON document and size(1024)
      StaticJsonDocument<1024> doc;
      String payload = theClient.getString();
      Serial.println("Parsing...");
      deserializeJson(doc, payload);

      DeserializationError error = deserializeJson(doc, payload);
      // Test if parsing succeeds.
      if (error) {
        Serial.print("deserializeJson() failed with error code ");
        Serial.println(error.c_str());
        Serial.println(payload);
        return;
      }

      //Some debugging lines below:
            Serial.println(payload);
      //      root.printTo(Serial);

      //Using .dot syntax, we refer to the variable "location" which is of
      //type GeoData, and place our data into the data structure.

      zipCode = doc["zip"].as<String>();
      Serial.println(zipCode);
    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
    }
  }
}

void getTemp() {
  mpl115a2.getPT(&pressureKPA,&temperatureC);
  Serial.print("Pressure (kPa): "); Serial.print(pressureKPA, 4); Serial.print(" kPa  ");
  Serial.print("Temp (*C): "); Serial.print(temperatureC, 1); Serial.println(" *C both measured together");
}

void publishInstruction(String output) {
  Serial.print("Publishing:");
  Serial.println(output);
  StaticJsonDocument<256> outputDoc;
  outputDoc["Instruction"] = output;
  char buffer[256];
  serializeJson(outputDoc, buffer);
  mqtt.publish(feed3, buffer);
}

void publishResults() {
  Serial.print("Publishing:");
  StaticJsonDocument<256> outputDoc;
  outputDoc["Patient"] = patientID;
  outputDoc["Call"] = call;
  outputDoc["Confidence"] = confidenceCall;
  char buffer[256];
  serializeJson(outputDoc, buffer);
  mqtt.publish(feed2, buffer);
}

void analyzeCall() {
  if (imageInputDoc.item1 == "Positive" or imageInputDoc.item1 == "Negative" or imageInputDoc.item1 == "Inconclusive") {
    call = imageInputDoc.item1;
  }
  else {
    call = "Error";
  }
  confidenceCall = imageInputDoc.p1;
  realCall = imageInputDoc.item1;
}

void RGB_color(String color)
 {
  if (color == "red") {digitalWrite(red_light_pin, LOW); digitalWrite(green_light_pin, HIGH); digitalWrite(blue_light_pin, HIGH);digitalWrite(led_off, HIGH);}
  if (color == "green") {digitalWrite(red_light_pin, HIGH); digitalWrite(green_light_pin, LOW); digitalWrite(blue_light_pin, HIGH);digitalWrite(led_off, HIGH);}
  if (color == "yellow") {digitalWrite(red_light_pin, LOW); digitalWrite(green_light_pin, LOW); digitalWrite(blue_light_pin, HIGH);digitalWrite(led_off, HIGH);}
  if (color == "blue") {digitalWrite(red_light_pin, HIGH); digitalWrite(green_light_pin, HIGH); digitalWrite(blue_light_pin, LOW);digitalWrite(led_off, HIGH);}
  if (color == "off") {digitalWrite(red_light_pin, HIGH); digitalWrite(green_light_pin, HIGH); digitalWrite(blue_light_pin, HIGH);digitalWrite(led_off, LOW);}
}

void LEDTest() {
  RGB_color("red");
  delay(500);
  RGB_color("green");
  delay(500);
  RGB_color("yellow");
  delay(500);
  RGB_color("blue");
  delay(500);
  RGB_color("off");
  delay(500);
}
