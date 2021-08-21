/*
 * CALLBACK
 * The callback is where we attacch a listener to the incoming messages from the server.
 * By subscribing to a specific channel or topic, we can listen to those topics we wish to hear.
 * We place the callback in a separate tab so we can edit it easier . . . (will not appear in separate
 * tab on github!)
 */

// The standard format at least in this library is that the mqtt server will always 
// send back a char and byte payload, and the total length:

void callback(char* topic, byte* payload, unsigned int length) {

  Serial.println("--------------------");
  Serial.print("A message arrived from [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");
  Serial.println();

  // If you are having trouble, lines 16-18 will print out the payload received
  //for (int i = 0; i < length; i++) {
  //  Serial.print((char)payload[i]);
  //}

  StaticJsonDocument<256> doc;

    /* The following is the basic way to deserialize the message payload:
        deserializeJson(doc, payload, length);

       But it gives no error reporting if there is a failure!
       Here's a better way, with error reporting too:
    */
    
  DeserializationError err = deserializeJson(doc, payload, length);

  if (err) { //well what was it?
    Serial.println("deserializeJson() failed, are you sure this message is JSON formatted?");
    Serial.print("Error code: ");
    Serial.println(err.c_str());
    return;
  }

  // For debugging or printing all messages
  //Serial.println("The message was: ");
  //serializeJsonPretty(doc, Serial);
  //Serial.println();

  /*
     We can use strcmp() -- string compare -- to check the incoming topic in case we need to do
     something based upon data in the subscribed topic, like move a servo or turn on a light

     strcmp(firstString, secondString) == 0 <-- '0' means NO differences; they are equivalent

     The fees in this example are: feed1, feed2, feed3 (which is listening for a button press)
  */
 // THis will parse the input of the Clarifai model
  if (strcmp(topic, feed1) == 0) {
    Serial.println("Image Analysis");
    Serial.print("Item 1: ");
    serializeJson(doc["Item1"]["Item"], Serial);
    Serial.print(": ");
    serializeJson(doc["Item1"]["p"], Serial);
    Serial.println();
    Serial.print("Item 2: ");
    serializeJson(doc["Item2"]["Item"], Serial);
    Serial.print(": ");
    serializeJson(doc["Item2"]["p"], Serial);
    Serial.println();
    Serial.print("Item 3: ");
    serializeJson(doc["Item3"]["Item"], Serial);
    Serial.print(": ");
    serializeJson(doc["Item3"]["p"], Serial);
    imageInputDoc.item1 = doc["Item1"]["Item"];
    imageInputDoc.p1 = doc["Item1"]["p"];
    imageInputDoc.item2 = doc["Item2"]["Item"];
    imageInputDoc.p2 = doc["Item2"]["p"];
    imageInputDoc.item3 = doc["Item3"]["Item"];
    imageInputDoc.p3 = doc["Item3"]["p"];
    if (imageInputDoc.item1 != "null") {
      captureResults = true;
    }
    else if (imageInputDoc.item1 == "null") {
      captureReset = true;
    }
  }
  else if (strcmp(topic, feed3) == 0) {
    if (doc["Instruction"] == "CameraOn") {
      cameraConnect = true; // Receive the signal the the camera is functional
    }
    if (doc["Instruction"] == "CameraOff") {
      cameraFail = true; // Receive the signal that the camera is not functional
    }
  }
  else if (strcmp(topic, feed2) == 0) {
    publishDoctor = true; // Receive information that results have been published to doctor
    Serial.println("Results 2");
  }  
}
