#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_ADXL345.h>
#include <SparkFun_APDS9960.h>
#include <WiFi.h>
#include <BleKeyboard.h>
//#include <BleGamepad.h>
//#include <BleMouse.h>

//Communication
ADXL345 adxl = ADXL345();
SparkFun_APDS9960 apds = SparkFun_APDS9960();
BleKeyboard bleKeyboard;

//WiFI config
const char* ssid = "Kwong";
const char* password = "26282628";
WiFiServer server(80);

//Constants
const uint8_t apdsInt[6] = {25, 26, 27, 17, 18, 19};
const uint8_t adxlLED[6] = {12, 13, 32, 33, 4, 5};
const uint8_t apdsLED = 2;

//GlobalVariables
uint8_t previousSelect = 6;
uint8_t apdsSelect = 0;
String header;
bool bleisReconnect, wifiisReconnect = false;
bool pressed = false;
bool wifiMenu = false;
int isr_flag = 0;

//Time
unsigned long markTime, currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

//Macro
String macro[6][6] =
{
  //0x
  {
    "VolumeUp",
    "VolumeDown",
    "Previous",
    "Next",
    "Stop",
    "Play/Pause"
  },
  //1x
  {
    "Up",
    "Sown",
    "Left",
    "Right",
    "Stop",
    "PlayPause"
  },
  //2x
  {
    "Copy",
    "Paste",
    "Undo",
    "Redo",
    "Stop",
    "Play/Pause"
  },
  //3x
  {
    "RotateUp",
    "RotateDown",
    "RotateLeft",
    "RotateRight",
    "Stop",
    "Play/Pause"
  },
  //4x
  {
    "Hold[w]",
    "Hold[s]",
    "Hold[a]",
    "Hold[d]",
    "Stop",
    "Play/Pause"
  },
  //5x
  {
    "PageUp",
    "PageDown",
    "TaskLeft",
    "TaskRight",
    "Stop",
    "Play/Pause"
  }
};

//Functions
void TCA9548A(int bus) {
  Wire.beginTransmission(0x70);
  Wire.write(1 << bus);
  Wire.endTransmission();
  Serial.println((String)"APDS9960(" + bus + ") is activated");
  isr_flag = 0;
}

//Interrupts
void interruptRoutine() {
  isr_flag = 1;
}

//Setup
void setup() {
  //Serial terminal
  Serial.begin(115200);

  //TCA9548A initialization
  Wire.begin();

  //LEDs initialization
  for (int i = 0; i < 6; i++) pinMode(adxlLED[i], OUTPUT);
  pinMode(apdsLED, OUTPUT);

  //ADXL345 Initialization
  Serial.println("/// Sensor Initialization ///");
  Serial.println("...");
  adxl.powerOn();
  adxl.setRangeSetting(2);
  adxl.setActivityXYZ(0, 0, 0);       // Set to activate movement detection in the axes "adxl.setActivityXYZ(X, Y, Z);" (1 == ON, 0 == OFF)
  adxl.setActivityThreshold(75);      // 62.5mg per increment   // Set activity   // Inactivity thresholds (0-255)

  adxl.setInactivityXYZ(0, 0, 0);     // Set to detect inactivity in all the axes "adxl.setInactivityXYZ(X, Y, Z);" (1 == ON, 0 == OFF)
  adxl.setInactivityThreshold(75);    // 62.5mg per increment   // Set inactivity // Inactivity thresholds (0-255)
  adxl.setTimeInactivity(10);         // How many seconds of no activity is inactive?

  //APDS9960 Initialization
  for (int i = 0; i < 6; i++) pinMode(apdsInt[i], INPUT_PULLUP);
  attachInterrupt(apdsInt[apdsSelect], interruptRoutine, FALLING);


  // Connect to Bluetooth device **(Start the Device THEN connect)**
  Serial.println("Starting BLE work!");
  bleKeyboard.begin();
  Serial.println("Waiting for connection");
  //  bleGamepad.begin();
  //  bleMouse.begin();

}

void loop() {
  if (bleKeyboard.isConnected()) {
    if (bleisReconnect) {
      bleisReconnect = false;
      bleKeyboard.startAdvertising();
      Serial.println("Connected");
      if (wifiMenu) {
        WiFi.begin(ssid, password);
        Serial.print("Connecting to ");
        Serial.println(ssid);
      }
    }
    if (wifiMenu) {
      if (WiFi.status() == WL_CONNECTED) {
        if (wifiisReconnect) {
          // Print local IP address and start web server
          wifiisReconnect = false;
          // Connect to Wi-Fi network with SSID and password
          Serial.println("");
          Serial.println("WiFi connected.");
          Serial.println("IP address: ");
          Serial.println(WiFi.localIP());
          server.begin();
        }
        //Webpage Template (NOT quite working)
        Webpage();
      }
      else wifiisReconnect = true;
    }
    
    //APDS Section
    APDS9960A();

    //ADXL Section
    ADXL345A();

  }
  
  else {
    bleisReconnect = true;
    Serial.print("...");
    markTime = millis();
    while (millis() - markTime < 1000); // Wait some time
    markTime = 0;
  }
}

//ADXL Section {ADXL345A(X,Y,Z)}
void ADXL345A() {
  //XYZ
  int x, y, z;
  float X, Y, Z;
  adxl.readAccel(&x, &y, &z);
  if (x > 35000) x = (x - 65336) * 7;
  if (y > 64000) y = y - 65536;
  if (z > 64000) z = z - 65536;
  X = (float)(x - 25) / 256;
  Y = (float)(y + 5) / 256;
  Z = (float)(z - 27) / 256;
  //      Serial.print(X);
  //      Serial.print(", ");
  //      Serial.print(Y);
  //      Serial.print(", ");
  //      Serial.println(Z);
  //SELECTOR
  if (X < -.9) apdsSelect = 3;
  else if (Y >  .9) apdsSelect = 1;
  else if (Y < -.9) apdsSelect = 4;
  else if (Z >  .9) apdsSelect = 0;
  else if (Z < -.9) apdsSelect = 5;
  else if ((X > .9) && (Z < .1) && (Z > -.1)) apdsSelect = 2;
  if ((apdsSelect != previousSelect) && (X < 1.2) && (Y < 1.2) && (Z < 1.2)) {
    bleKeyboard.releaseAll();
    pressed = false;
    if (previousSelect != 6 ) detachInterrupt(apdsInt[previousSelect]);
    TCA9548A(apdsSelect);
    apds.init();
    apds.enableGestureSensor(true);
    if (previousSelect != 6 ) attachInterrupt(apdsInt[apdsSelect], interruptRoutine, FALLING);
    previousSelect = apdsSelect;
    for (int i = 0; i < 6; i++) digitalWrite(adxlLED[i], LOW);
    digitalWrite(adxlLED[apdsSelect], HIGH);
    markTime = millis();
    isr_flag = 0;
  }
  // Reopen APDS if stuck
  if ((markTime != 0) && (millis() - markTime > 1000)) {
    apds.init();
    apds.enableGestureSensor(true);
    markTime = 0;
  }
}

//APDS Section
void APDS9960A() {
  if ( isr_flag == 1 ) Gesturehandle();
  detachInterrupt(apdsInt[apdsSelect]);
  isr_flag = 0;
  attachInterrupt(apdsInt[apdsSelect], interruptRoutine, FALLING);
}

void Gesturehandle() {
  if ( apds.isGestureAvailable() ) {
    switch (apdsSelect) {
      case 0:
        switch ( apds.readGesture() ) {
          case DIR_UP:
            Serial.println("UP");
            bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
            break;
          case DIR_DOWN:
            Serial.println("DOWN");
            bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
            break;
          case DIR_LEFT:
            Serial.println("LEFT");
            bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
            break;
          case DIR_RIGHT:
            Serial.println("RIGHT");
            bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
            break;
          case DIR_NEAR:
            Serial.println("NEAR");
            bleKeyboard.write(KEY_MEDIA_STOP);
            break;
          case DIR_FAR:
            Serial.println("FAR");
            bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
            break;
          default: {}
        }
        break;
      case 2:
        switch ( apds.readGesture() ) {
          case DIR_UP:
            Serial.println("UP");
            bleKeyboard.press(KEY_LEFT_CTRL);
            bleKeyboard.press('c');
            bleKeyboard.releaseAll();
            break;
          case DIR_DOWN:
            Serial.println("DOWN");
            bleKeyboard.press(KEY_LEFT_CTRL);
            bleKeyboard.press('v');
            bleKeyboard.releaseAll();
            break;
          case DIR_LEFT:
            Serial.println("LEFT");
            bleKeyboard.press(KEY_LEFT_CTRL);
            bleKeyboard.press('z');
            bleKeyboard.releaseAll();
            break;
          case DIR_RIGHT:
            Serial.println("RIGHT");
            bleKeyboard.press(KEY_LEFT_CTRL);
            bleKeyboard.press(KEY_LEFT_SHIFT);
            bleKeyboard.press('z');
            bleKeyboard.releaseAll();
            break;
          case DIR_NEAR:
            Serial.println("NEAR");
            bleKeyboard.write(KEY_MEDIA_STOP);
            break;
          case DIR_FAR:
            Serial.println("FAR");
            bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
            break;
          default: {}
        }
        break;
      case 5:
        switch ( apds.readGesture() ) {
          case DIR_UP:
            Serial.println("UP");
            bleKeyboard.press(KEY_PAGE_UP);
            bleKeyboard.releaseAll();
            break;
          case DIR_DOWN:
            Serial.println("DOWN");
            bleKeyboard.press(KEY_PAGE_DOWN);
            bleKeyboard.releaseAll();
            break;
          case DIR_LEFT:
            Serial.println("LEFT");
            bleKeyboard.press(KEY_LEFT_ALT);
            bleKeyboard.press(KEY_TAB);
            bleKeyboard.write(KEY_LEFT_ARROW);
            bleKeyboard.releaseAll();
            break;
          case DIR_RIGHT:
            Serial.println("RIGHT");
            bleKeyboard.press(KEY_LEFT_ALT);
            bleKeyboard.press(KEY_TAB);
            bleKeyboard.write(KEY_RIGHT_ARROW);
            bleKeyboard.releaseAll();
            break;
          case DIR_NEAR:
            Serial.println("NEAR");
            bleKeyboard.write(KEY_MEDIA_STOP);
            break;
          case DIR_FAR:
            Serial.println("FAR");
            bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
            break;
          default: {}
        }
        break;
      case 4:
        switch ( apds.readGesture() ) {
          case DIR_UP:
            Serial.println("UP");
            bleKeyboard.press('w');
            pressed = true;
            break;
          case DIR_DOWN:
            Serial.println("DOWN");
            bleKeyboard.press('s');
            pressed = true;
            break;
          case DIR_LEFT:
            Serial.println("LEFT");
            bleKeyboard.press('a');
            pressed = true;
            break;
          case DIR_RIGHT:
            Serial.println("RIGHT");
            bleKeyboard.press('d');
            pressed = true;
            break;
          case DIR_NEAR:
            Serial.println("NEAR");
            if (!pressed) bleKeyboard.write(KEY_MEDIA_STOP);
            else {
              bleKeyboard.releaseAll();
              pressed = false;
            }
            break;
          case DIR_FAR:
            Serial.println("FAR");
            if (!pressed) bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
            else {
              bleKeyboard.releaseAll();
              pressed = false;
            }
            break;
          default: {}
        }
        break;
      case 1:
        switch ( apds.readGesture() ) {
          case DIR_UP:
            Serial.println("UP");
            bleKeyboard.write(KEY_UP_ARROW);
            break;
          case DIR_DOWN:
            Serial.println("DOWN");
            bleKeyboard.write(KEY_DOWN_ARROW);
            break;
          case DIR_LEFT:
            Serial.println("LEFT");
            bleKeyboard.write(KEY_LEFT_ARROW);
            break;
          case DIR_RIGHT:
            Serial.println("RIGHT");
            bleKeyboard.write(KEY_RIGHT_ARROW);
            break;
          case DIR_NEAR:
            Serial.println("NEAR");
            bleKeyboard.write(KEY_MEDIA_STOP);
            bleKeyboard.releaseAll();
            break;
          case DIR_FAR:
            Serial.println("FAR");
            bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
            bleKeyboard.releaseAll();
            break;
          default: {}
        }
        break;
      case 3:
        switch ( apds.readGesture() ) {
          case DIR_UP:
            Serial.println("UP");
            bleKeyboard.press(KEY_LEFT_CTRL);
            bleKeyboard.write(KEY_UP_ARROW);
            bleKeyboard.releaseAll();
            break;
          case DIR_DOWN:
            Serial.println("DOWN");
            bleKeyboard.press(KEY_LEFT_CTRL);
            bleKeyboard.write(KEY_DOWN_ARROW);
            bleKeyboard.releaseAll();
            break;
          case DIR_LEFT:
            Serial.println("LEFT");
            bleKeyboard.press(KEY_LEFT_CTRL);
            bleKeyboard.write(KEY_LEFT_ARROW);
            bleKeyboard.releaseAll();
            break;
          case DIR_RIGHT:
            Serial.println("RIGHT");
            bleKeyboard.press(KEY_LEFT_CTRL);
            bleKeyboard.write(KEY_RIGHT_ARROW);
            bleKeyboard.releaseAll();
            break;
          case DIR_NEAR:
            Serial.println("NEAR");
            bleKeyboard.write(KEY_MEDIA_STOP);
            bleKeyboard.releaseAll();
            break;
          case DIR_FAR:
            Serial.println("FAR");
            bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
            bleKeyboard.releaseAll();
            break;
          default: {}
        }
        break;
    }
    digitalWrite(apdsLED, HIGH);
    markTime = millis();
    while (millis() - markTime < 250); // Wait some time
    markTime = 0;
    apds.init();
    apds.enableGestureSensor(true);
    digitalWrite(apdsLED, LOW);
  }
}

//NOT quite working ( going to use ---> javaScript Ajax.)
void Webpage() {

  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            //
            //            // Basic controls
            //            if (bleKeyboard.isConnected()) {
            //              if (header.indexOf("GET /Play_Pause") >= 0) {
            //                Serial.println("Play/Pause");
            //                bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
            //              } else if (header.indexOf("GET /Next") >= 0) {
            //                Serial.println("Next");
            //                bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
            //              } else if (header.indexOf("GET /Previous") >= 0) {
            //                Serial.println("Previous");
            //                bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
            //              }
            //            }

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>BLE Gesture Controller User Manual</h1>");

            client.println("<h2>Try Rotating your controller to find out what function are on that face.</h2>");

            // Face Indicator
            client.println((String)"<p>Now in Face [" + (apdsSelect + 1) + "]</p>");
            client.println();
            client.println();

            // UP
            client.println("<p>UP</p>");
            client.println("<p><a href=\"/UP\"><button class=\"button\">" + macro[apdsSelect][0] + "</button></a></p>");

            // DOWN
            client.println("<p>DOWN</p>");
            client.println("<p><a href=\"/DOWN\"><button class=\"button\">" + macro[apdsSelect][1] + "</button></a></p>");

            // LEFT
            client.println("<p>LEFT</p>");
            client.println("<p><a href=\"/LEFT\"><button class=\"button\">" + macro[apdsSelect][2] + "</button></a></p>");


            // RIGHT
            client.println("<p>RIGHT</p>");
            client.println("<p><a href=\"/RIGHT\"><button class=\"button\">" + macro[apdsSelect][3] + "</button></a></p>");


            // NEAR
            client.println("<p>NEAR</p>");
            client.println("<p><a href=\"/NEAR\"><button class=\"button\">" + macro[apdsSelect][4] + "</button></a></p>");


            // FAR
            client.println("<p>FAR</p>");
            client.println("<p><a href=\"/FAR\"><button class=\"button\">" + macro[apdsSelect][5] + "</button></a></p>");

            client.println("</body></html>");

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}
