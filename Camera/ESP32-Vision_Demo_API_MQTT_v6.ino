#include "Arduino.h"
#include "esp_camera.h"
#include <WiFi.h>
#include <base64.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Button.h>
#include <PubSubClient.h>   //
#include "config.h" // include the configuration of credentials in the config.h file

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE  // Has PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM

#include "camera_pins.h"

// Setup the pins and the button
int flash = 4;
int indicator = 33;
int buttonPin = 13;
int buttonState = 0;
Button btn (buttonPin);
bool flash_on = true;
bool cameraOn = false;

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
} imageOut;     //then we give our new data structure a name so we can use it in our code

imageOut imageOutputDoc;

bool captureImage = false;

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
  // Setup the pins for the flash and buttons  
  pinMode(flash, OUTPUT);
  pinMode(indicator, OUTPUT);
  pinMode(buttonPin, INPUT);
  // Camera configurations
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  Serial.println("Camera Initialized");
  
#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif
// Connect to the wifi  
  connectWifi();
  mqtt.setServer(mqtt_server, mqtt_port); // setup the MQTT object
  mqtt.setCallback(callback);
  cameraOn = true;
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }

  mqtt.loop(); //this keeps the mqtt connection 'active'
  // Allows for the capture of an image with the button or when receives capture image
  int action = btn.checkButtonAction();
  if (action == Button::CLICKED or captureImage) {
    Serial.println("Classifying image...");
    classifyImage();
    publishImage();
    captureImage = false;
  } 
// Hold button toggles flash  
  else if (action == Button::HELD_CLICKED) {
    Serial.print("button on pin ");
    Serial.println(": held-clicked");
    flash_on = !flash_on;
    digitalWrite(indicator, HIGH);
    delay(100);
    digitalWrite(indicator, LOW);
    if (flash_on) {
      delay(100);
      digitalWrite(indicator, HIGH);
      delay(100);
      digitalWrite(indicator, LOW);
    }
  } else {
    // nothing happened with the button, so do nothing
  }
}

// Main function to classify the image and connect to clarifai
void classifyImage() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(500);
    connectWifi();
  }
  // Flash
  if(flash_on) {
    digitalWrite(4, HIGH);
    delay(100);
  }
  // Capture picture
   camera_fb_t * fb = NULL;
   fb = esp_camera_fb_get();
   delay(100);
   digitalWrite(4, LOW);
   
   if(!fb) {
    Serial.println("Camera capture failed");
    return;
   }

   Serial.println("Capturing image");
  // Convert the image to base64 and then make the payload
  size_t size = fb->len;
  String buffer = base64::encode((uint8_t *) fb->buf, fb->len);
  String payload = "{\"inputs\": [{ \"data\": {\"image\": {\"base64\": \"" + buffer + "\"}}}]}";

  buffer = "";
  // Uncomment this if you want to show the payload
  Serial.println(payload);

  esp_camera_fb_return(fb);
  
  // Below are IDs for the different models from clarifai
    // ***Food Model
  String model_id = "9504135848be0dd2c39bdab0002f78e9";
  String key_id  = "Key baba5ad200fb406691563b8c6d7b849e";
    // ***Face Model
  //String model_id = "1e08e61c48404a269950b69317c520d4";
  //String key_id  = "Key baba5ad200fb406691563b8c6d7b849e";
    // ***Color Model
  //String model_id = "eeed0b6733a644cea07cf4c60f87ebb7";
  //String key_id  = "Key baba5ad200fb406691563b8c6d7b849e";

  // This will send the HTTP POST call to the model
  HTTPClient http;
  http.begin("https://api.clarifai.com/v2/models/" + model_id + "/outputs");
  http.addHeader("Content-Type", "application/json");     
  http.addHeader("Authorization", key_id); 
  int response_code = http.POST(payload);
  Serial.println(response_code);
  
  // Parse the json response: Arduino assistant
  //int httpCode = http.GET();
  //if (httpCode > 0) {
    //if (httpCode == 200) {
      Serial.println("Received HTTP payload.");
      DynamicJsonDocument doc(ESP.getMaxAllocHeap()); // specify JSON document and size(1024)
      //StaticJsonDocument<1024> doc;
      String payload_in = http.getString();
      Serial.println("Parsing...");
      deserializeJson(doc, payload_in);
      serializeJsonPretty(doc, Serial);
      

      DeserializationError error = deserializeJson(doc, payload_in);
      // Test if parsing succeeds.
      if (error) {
        Serial.print("deserializeJson() failed with error code ");
        Serial.println(error.c_str());
        Serial.println(payload_in);
        //return;
      }
      imageOutputDoc.item1 = doc["outputs"][0]["data"]["concepts"][0]["name"];
      imageOutputDoc.p1 = doc["outputs"][0]["data"]["concepts"][0]["value"];
      imageOutputDoc.item2 = doc["outputs"][0]["data"]["concepts"][1]["name"];
      imageOutputDoc.p2 = doc["outputs"][0]["data"]["concepts"][1]["value"];
      imageOutputDoc.item3 = doc["outputs"][0]["data"]["concepts"][2]["name"];
      imageOutputDoc.p3 = doc["outputs"][0]["data"]["concepts"][2]["value"];
      
      for (int i=0; i < 10; i++) {
        const String item = doc["outputs"][0]["data"]["concepts"][i]["name"];
        const float p = doc["outputs"][0]["data"]["concepts"][i]["value"];
    
        Serial.println("=====================");
        Serial.print("Name:");
        Serial.println(item);
        Serial.print("Prob:");
        Serial.println(p);
        Serial.println();
      }
    //}
  //}
  Serial.println("\nSleep....");
  //esp_deep_sleep_start();
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
// Reconnect to wifi if disconnected
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
// Publish the image information to the feed
void publishImage() {
  StaticJsonDocument<256> outputDoc;
  outputDoc["Item1"]["Item"] = imageOutputDoc.item1;
  outputDoc["Item1"]["p"] = imageOutputDoc.p1;
  outputDoc["Item2"]["Item"] = imageOutputDoc.item2;
  outputDoc["Item2"]["p"] = imageOutputDoc.p2;
  outputDoc["Item3"]["Item"] = imageOutputDoc.item3;
  outputDoc["Item3"]["p"] = imageOutputDoc.p3;
  char buffer[256];
  serializeJson(outputDoc, buffer);
  mqtt.publish(feed1, buffer);
}
// Publish the information for communication
void publishInstruction(String output) {
  StaticJsonDocument<256> outputDoc;
  outputDoc["Instruction"] = output;
  char buffer[256];
  serializeJson(outputDoc, buffer);
  mqtt.publish(feed3, buffer);
}
