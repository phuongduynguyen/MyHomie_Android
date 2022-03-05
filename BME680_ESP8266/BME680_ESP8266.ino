/* D1 (GPIO5): SCL
 * D2 (GPIO4): SDA
   D5 (GPIO14): buzzer
*/

#include <Wire.h>
#include "bsec.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "FirebaseESP8266.h"
#include <ArduinoJson.h>
#include <String.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <time.h>

#include "SH1106Wire.h"

//#include "SSD1306.h"


#define SEALEVELPRESSURE_HPA (1013.25)
#define FIREBASE_HOST "myhomie-72e49-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "WJfbu2S9GVfHcwDIgBvZcbYFhv2LBiiPmsoSro7T"
//
//#define FIREBASE_HOST "smarthome-a8b99-default-rtdb.firebaseio.com"
//#define FIREBASE_AUTH "AhQ1BUCtZWL3efXyHjcnC27OH2OtanlP7joPgFld"
#define BUZ 14

FirebaseData firebaseData;
FirebaseJson json;
String path      = "/";

Bsec iaqSensor;
String output;
void checkIaqSensorStatus(void);
void errLeds(void);
void draw();
void time_line();


const IPAddress apIP(192, 168, 1, 1);
const char* apSSID = "MONITOR_SETUP";
boolean settingMode;
String ssidList;          // Bien liet ke cac mang wifi xung quanh

DNSServer dnsServer;
ESP8266WebServer webServer(80);


// Initialize the OLED display using Wire library
SH1106Wire display(0x3c, D2, D1);
//SSD1306  display(0x3c, 4, 5);


int timezone = 7 * 3600; // 7  time in jakarta
int dst = 0;


void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Wire.begin();
  EEPROM.begin(512);
  pinMode(BUZ, OUTPUT);

  iaqSensor.begin(BME680_I2C_ADDR_SECONDARY, Wire);
  output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);

  Serial.println(output);
  checkIaqSensorStatus();
  bsec_virtual_sensor_t sensorList[10] = {
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };
 
  iaqSensor.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();

  display.init();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Hello world");
  display.display();
  display.clear();


  if (restoreConfig())      //neu co du lieu wifi trong EEPROM thi ket noi va kiem tra ket noi voi wifi do
  {                       
    if (checkConnection())
    {
      settingMode = false;  //set che do cai dat la false de bo qua buoc settingMode
      Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
      Firebase.reconnectWiFi(true);
      startWebServer(); //khoi tao 1 webserver de truy cap
      configTime(timezone, dst, "pool.ntp.org","time.nist.gov");
      while(!time(nullptr))
      {
        delay(1000);
      }
      return;                                   
    }
  }
  settingMode = true;                           // set che do cai dat lai thanh true
  setupMode();    
}
 
void loop()
{
 // put your main code here, to run repeatedly
 // server.handleClient(); 
  if (settingMode)      //neu settingMode == 1 thi 
  {                      
    dnsServer.processNextRequest();
  }
  webServer.handleClient();
  if (iaqSensor.run())
  {
    // If new data is available 
  }
  else
  {
    checkIaqSensorStatus();
  }
 
  display.clear();
  display.setFont(ArialMT_Plain_10);
  if ((iaqSensor.staticIaq > 0)  && (iaqSensor.staticIaq  <= 50))
  {
   // IAQsts = "Good"; 
    display.drawString(0, 45, "IAQ     : Good"   );
  }
  else if ((iaqSensor.staticIaq > 51)  && (iaqSensor.staticIaq  <= 100))
  {
 //   IAQsts = "Average";
    display.drawString(0, 45, "IAQ     : Average"   );
  }
  else if ((iaqSensor.staticIaq > 101)  && (iaqSensor.staticIaq  <= 150))
  {
   // IAQsts = "Little Bad";
    display.drawString(0, 45, "IAQ     : Little Bad" );
  }
  else if ((iaqSensor.staticIaq > 151)  && (iaqSensor.staticIaq  <= 200))
  {
  //  IAQsts = "Bad";
    display.drawString(0, 45, "IAQ     : Bad"  );
  }
  else if ((iaqSensor.staticIaq > 201)  && (iaqSensor.staticIaq  <= 300))
  {
    //IAQsts = "Worse";
    display.drawString(0, 45, "IAQ     : Worse" );
  }
  else if ((iaqSensor.staticIaq > 301))
  {
   // IAQsts = "Very Bad";
    display.drawString(0,45, "IAQ     : Very Bad"  );
  } 

  if((iaqSensor.temperature)>=70 || iaqSensor.gasResistance <= 12000 )
  {
    Firebase.setString(firebaseData, path + "/Alert", "ON");
    digitalWrite(BUZ,1);   
  }
  else
  {
    digitalWrite(BUZ,0);
    Firebase.setString(firebaseData, path + "/Alert", "OFF");
  }
  
  Firebase.setString(firebaseData, path + "/IAQ", String(iaqSensor.staticIaq));
  Firebase.setString(firebaseData, path + "/NhietDo", String(iaqSensor.temperature));
  Firebase.setString(firebaseData, path + "/DoAm", String(iaqSensor.humidity));
  Firebase.setString(firebaseData, path + "/Co2", String(iaqSensor.co2Equivalent));
  Firebase.setString(firebaseData, path + "/ApSuat", String(iaqSensor.pressure  ));
 
  display.drawString(0, 0,  "Temp  :" + String(iaqSensor.temperature) + "°C" );
  display.drawString(0, 15, "Humid :" + String(iaqSensor.humidity) + "%" );
  display.drawString(0, 30, "Press  :" + String(int(iaqSensor.pressure / 1000)) + "hPa");
//  Serial.println(iaqSensor.gasResistance);
  draw();
  time_line();
  display.display();
}

void draw()
{
  display.drawString(74, 0, "|"  );
  display.drawString(74, 5, "|"  );
  display.drawString(74, 10, "|"  );
  display.drawString(74, 15, "|"  );
  display.drawString(74, 20, "|"  );
  display.drawString(74, 25, "|"  );
  display.drawString(74, 30, "|"  );
  display.drawString(74, 35, "|"  );
  display.drawString(74, 40, "|"  );
  display.drawString(74, 45, "|"  );

}

void time_line()
{
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  display.drawString(78, 30, String(p_tm->tm_mday));
  display.drawString(90, 30, "/");
  display.drawString(93, 30, String(p_tm->tm_mon + 1));
  display.drawString(101, 30, "/");
  display.drawString(105, 30, String(p_tm->tm_year + 1900));
  display.setFont(ArialMT_Plain_16);
  display.drawString(80, 0, String(p_tm->tm_hour));    
  display.drawString(103, 0, ":");
  display.drawString(110, 0, String(p_tm->tm_min));
}

// Helper function definitions
void checkIaqSensorStatus(void)
{
  if (iaqSensor.status != BSEC_OK)
  {
    if (iaqSensor.status < BSEC_OK)
    {
      output = "BSEC error code : " + String(iaqSensor.status);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    } 
    else
    {
      output = "BSEC warning code : " + String(iaqSensor.status);
      Serial.println(output);
    }
  }
 
  if (iaqSensor.bme680Status != BME680_OK)
  {
    if (iaqSensor.bme680Status < BME680_OK)
    {
      output = "BME680 error code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    } 
    else
    {
      output = "BME680 warning code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
    }
  }
}

void errLeds(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
}

boolean restoreConfig()
{
  //Serial.println("Reading EEPROM...");
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 10, "Reading EEPROM...");
  display.display();
  delay(1500);
  display.clear();
  
  String ssid = "";
  String pass = "";
  if (EEPROM.read(0) != 0)            //neu duu lieu doc ra tu EEPROM khac 0 thi doc du lieu
  {                      
    for (int i = 0; i < 32; ++i)      //32 o nho dau tieu la chua ten mang wifi SSID
    {                
      ssid += char(EEPROM.read(i));
    }
//    Serial.print("SSID: ");
//    Serial.println(ssid);


    for (int i = 32; i < 96; ++i)       //o nho tu 32 den 96 la chua PASSWORD
    {               
      pass += char(EEPROM.read(i));
    }
    
//    Serial.print("Password: ");
//    Serial.println(pass);

    WiFi.begin(ssid.c_str(), pass.c_str());       //ket noi voi mang WIFI duoc luu trong EEPROM
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 10, "SSID:" + ssid);
    display.drawString(0, 20, "PASS:" + pass);
    display.display();
    delay(5000);
    return true;
  }
  else 
  { 
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 10, "Config not found!");
    display.display();
    return false;
  }
}



//-----------Kiem tra lai ket noi voi WIFI-----------------------
boolean checkConnection()
{
  int count = 0;
//  Serial.print("Waiting for Wi-Fi connection");
  
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 10, "Waiting for Wi-Fi connection");
  display.display();

  while ( count < 30 )
  {
    if (WiFi.status() == WL_CONNECTED)          //neu ket noi thanh cong thi in ra connected!
    {     
        display.clear();
        display.setFont(ArialMT_Plain_10);
        display.drawString(0, 10, "Connected!");
        display.display();
        return (true);
    }
    delay(500);
    Serial.print(".");
    count++;
  }
  Serial.println("Timed out.");
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 10, "Timed out!");
  display.display();
  return false;
}


//-----------------Thiet lap mot WEBSERVER-------------------------------
void startWebServer()
{
  if (settingMode)                                //neu o chua settingMode la true thi thiet lap 1 webserver
  {                            
     display.clear();
     display.setFont(ArialMT_Plain_10);
     display.drawString(0, 10, "Starting Web Server at:");
     display.drawString(0, 25, WiFi.softAPIP().toString());
     display.display();
   
    webServer.on("/settings", []() {
      String s = "<h1>Wi-Fi Settings</h1><p>Please enter your password by selecting the SSID.</p>";
      s += "<form method=\"get\" action=\"setap\"><label>SSID: </label><select name=\"ssid\">";
      s += ssidList;
      s += "</select><br>Password: <input name=\"pass\" length=64 type=\"password\"><input type=\"submit\"></form>";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
    });
 
    webServer.on("/setap", []() {
      for (int i = 0; i < 96; ++i) {
        EEPROM.write(i, 0);               //xoa bo nho EEPROM
      }

      String ssid = urlDecode(webServer.arg("ssid"));

      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 10, "SSID: " + ssid);
      display.display();
        
      String pass = urlDecode(webServer.arg("pass"));
      
      display.drawString(0, 20, "PASS: " + pass);
      display.display();
      delay(1500);
      display.clear();
      display.drawString(0, 10, "Writing SSID to EEPROM...");
      display.display();
      
      for (int i = 0; i < ssid.length(); ++i)
      {
        EEPROM.write(i, ssid[i]);
      }
      
      display.clear();
      display.drawString(0, 10, "Writing Password to EEPROM...");
      for (int i = 0; i < pass.length(); ++i)
      {
        EEPROM.write(32 + i, pass[i]);
      }

      display.clear();
      EEPROM.commit();
      display.drawString(0, 10, "Write EEPROM done!");

      String s = "<h1>Setup complete.</h1><p>device will be connected to \"";
      s += ssid;
      s += "\" after the restart.";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
      ESP.restart();
    });
    webServer.onNotFound([]() {
      String s = "<h1>AP mode</h1><p><a href=\"/settings\">Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("AP mode", s));
    });
  }
  else 
  {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 10, "Starting Web Server at:");
    display.drawString(0, 25, WiFi.localIP().toString());
    display.display();
    delay(4000);
    display.clear();
    Firebase.setString(firebaseData, path + "/MONITOR_IP_STA", WiFi.localIP().toString());

    webServer.on("/", []() {
      String s = "<h1>STA mode</h1><p><a href=\"/reset\">Reset Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("STA mode", s));
    });
    
    webServer.on("/reset", []() {    // kiem tra duong dan"/reset" thi xoa EEPROM
      for (int i = 0; i < 96; ++i) {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
      String s = "<h1>Wi-Fi settings was reset.</h1><p>Please reset device.</p>";
      webServer.send(200, "text/html", makePage("Reset Wi-Fi Settings", s));
      ESP.restart();
    });
          
  }
  webServer.begin();
}



//-----------------Che do cai dat wifi cho esp8266----------------------
void setupMode()
{
  WiFi.mode(WIFI_STA);            //che do hoat dong la May Tram Station
  WiFi.disconnect();              //ngat ket noi wifi
  delay(100);
  int n = WiFi.scanNetworks();    //quet cac mang wifi xung quanh xem co bao nhieu mang
  delay(100);
  Serial.println("");
  for (int i = 0; i < n; ++i)     //dua danh sach wifi vao list
  {  
    ssidList += "<option value=\"";
    ssidList += WiFi.SSID(i);
    ssidList += "\">";
    ssidList += WiFi.SSID(i);
    ssidList += "</option>";
  }
  delay(100);
  WiFi.mode(WIFI_AP);               // chuyen sang che dong Access point
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSSID,"12345678");              //thiet lap 1 open netword WiFi.softAP(ssid, password)
  dnsServer.start(53, "*", apIP);
  startWebServer();
 
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 10, "Starting Access Point");
  display.drawString(0, 20, "SSID:MONITOR_SETUP");
  display.drawString(0,30 , "PASS:12345678");
  display.display();
  delay(5000);
}

String makePage(String title, String contents)
{
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += "</body></html>";
  return s;
}

String urlDecode(String input)
{
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}
