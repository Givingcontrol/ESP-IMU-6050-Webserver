/*20241203
 *  Source: https://ocw.cs.pub.ro/courses/iothings/proiecte/2021/earthquakedetection
 *  Complile error: Cannot complile for Node ESp32S
 *  include Little File System <LittleFS.h>, comment out SPIFFS.h (superceded by LittleFS)
 *  change all SPIFFS referneces to LittleFS
    change to new IDE v2.3.3 (Auto updates libraries)
    compiles OK
    Load LittleFS plugin:
      https://github.com/earlephilhower/arduino-littlefs-upload?tab=readme-ov-file#installation

 *  I2C
    GPIO 22 SCL
    GPIO 21 SDA
    3.3V
    0V

    I2C device found at address 0x3C =SSD1306
    I2C device found at address 0x68 = MPU6050
    I2C device found at address 0x76  = BMP/E280  20201203 Not coded in

    Added Access Point Code to WiFiInit

  V2
  Add summing of vectors for total force: int Sumaccel = (sqrt((a.acceleration.x*a.acceleration.x)+(a.acceleration.y*a.acceleration.y)+(a.acceleration.z*a.acceleration.z)));
    get values on OLED
    get values on webpage  (Needed CRTL+F5 to force cache clear)

  V3 - Try to draw Graph  with smoothie.js
        http://smoothiecharts.org/
            changes made in index.html and script.js
 */ 




#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <Adafruit_SSD1306.h>
// Replace with your network credentials
const char* ssid = "Clicknet-3E79";
const char* password = "KCLXY27UASMZF";

//const char* ssid = "AndroidAPEF43";
//const char* password = "ghghghgh";


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Json Variable to Hold Sensor Readings
JSONVar readings;

// Timer variables initialize
unsigned long lastTime = 0;  
unsigned long lastTimeTemperature = 0;
unsigned long lastTimeAcc = 0;
unsigned long gyroDelay = 10;
unsigned long temperatureDelay = 1000;
unsigned long accelerometerDelay = 20; // OG=200

// Create a sensor object
Adafruit_MPU6050 mpu;
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);
sensors_event_t a, g, temp;
sensors_event_t old_a, old_g, old_temp;
float gyroX, gyroY, gyroZ;
float accX, accY, accZ, Sumaccel;
float temperature;

//Gyroscope sensor deviation
//float gyroXerror = 0.07;
//float gyroYerror = 0.03;
//float gyroZerror = 0.01;

float gyroXerror = 0.08;
float gyroYerror = 0.02;
float gyroZerror = 0.015;

// Init MPU6050
void initMPU(){
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");
}

void initSPIFFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

// Initialize WiFi
void initWiFi() {
  /*
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");
  Serial.println(WiFi.localIP());

*/
//Access point
   // You can remove the password parameter if you want the AP to be open.
  WiFi.softAP(ssid); //, password
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

}




String getGyroReadings(){
  mpu.getEvent(&a, &g, &temp);
  old_a=a;
  old_g=g;
  old_temp=temp;
  float gyroX_temp = g.gyro.x;
  if(abs(gyroX_temp) > gyroXerror)  {
    gyroX += gyroX_temp/50.00;
  }
  
  float gyroY_temp = g.gyro.y;
  if(abs(gyroY_temp) > gyroYerror) {
    gyroY += gyroY_temp/70.00;
  }

  float gyroZ_temp = g.gyro.z;
  if(abs(gyroZ_temp) > gyroZerror) {
    gyroZ += gyroZ_temp/90.00;
  }

  readings["gyroX"] = String(gyroX);
  readings["gyroY"] = String(gyroY);
  readings["gyroZ"] = String(gyroZ);

  String jsonString = JSON.stringify(readings);
  return jsonString;
}

String getAccReadings() {
  mpu.getEvent(&a, &g, &temp);
  // Get current acceleration values
  accX = a.acceleration.x;
  accY = a.acceleration.y;
  accZ = a.acceleration.z;
  Sumaccel = (sqrt((a.acceleration.x*a.acceleration.x)+(a.acceleration.y*a.acceleration.y)+(a.acceleration.z*a.acceleration.z)));
  
  readings["accX"] = String(accX);
  readings["accY"] = String(accY);
  readings["accZ"] = String(accZ);
  readings["Sumaccel"] = String(Sumaccel);

  String accString = JSON.stringify (readings);
  return accString;
}

String getTemperature(){
  mpu.getEvent(&a, &g, &temp);
  temperature = temp.temperature;
  return String(temperature);
}

void setup() {
  Serial.begin(115200);
  initWiFi();
  initSPIFFS();
  initMPU();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    

  }
  display.display();
  delay(500); // Pause for 2 seconds
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setRotation(0);


  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    gyroX=0;
    gyroY=0;
    gyroZ=0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetX", HTTP_GET, [](AsyncWebServerRequest *request){
    gyroX=0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetY", HTTP_GET, [](AsyncWebServerRequest *request){
    gyroY=0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetZ", HTTP_GET, [](AsyncWebServerRequest *request){
    gyroZ=0;
    request->send(200, "text/plain", "OK");
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  server.begin();
}

void loop() {
  
  sensors_event_t a, g, temp;
  sensors_event_t old_a, old_g, old_temp;
  old_a=a;
  old_g=g;
  old_temp=old_temp;
  mpu.getEvent(&a, &g, &temp);
//  if (old_g.acceleration.x-g.acceleration.x >0.4){
//    Serial.println("Cutremur");
//  }
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Accelerometer - m/s^2");
  display.print(a.acceleration.x, 1);
  display.print(", ");
  display.print(a.acceleration.y, 1);
  display.print(", ");
  display.print(a.acceleration.z, 1);
  display.print(", ");
  display.print((sqrt((a.acceleration.x*a.acceleration.x)+(a.acceleration.y*a.acceleration.y)+(a.acceleration.z*a.acceleration.z)))/10, 1);
  display.println("");
  display.println("Gyroscope - rps");
  display.print(g.gyro.x, 1);
  display.print(", ");
  display.print(g.gyro.y, 1);
  display.print(", ");
  display.print(g.gyro.z, 1);
  display.println("");
  display.display();
  delay(100);
  if ((millis() - lastTime) > gyroDelay) {
    // Send Events to the Web Server with the Sensor Readings
    events.send(getGyroReadings().c_str(),"gyro_readings",millis());
    lastTime = millis();
  }
  if ((millis() - lastTimeAcc) > accelerometerDelay) {

    // Send Events to the Web Server with the Sensor Readings
    events.send(getAccReadings().c_str(),"accelerometer_readings",millis());
    lastTimeAcc = millis();
  }
  if ((millis() - lastTimeTemperature) > temperatureDelay) {
    // Send Events to the Web Server with the Sensor Readings
    events.send(getTemperature().c_str(),"temperature_reading",millis());
    lastTimeTemperature = millis();
  }
}
