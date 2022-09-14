/*
  Heating controller for multiple zones. Designed for EPS32, uses MQTT over WiFi.

  Based on: https://create.arduino.cc/projecthub/erkr/multi-zone-heating-controller-41d40c?ref=user&ref_id=232633&offset=0

  But with major additions for ESP32/WiFi/MQTT and customised to my needs.

  Uses thermostats or MQTT to switch zones on/off. MQTT is designed to be used with HomeAssistant or similar.

  Supports OTA updating of the ESP32

  Apache License 2.0, Andrew Berridge, 2022

*/

#include "SSD1306.h"  // ThingPulse ESP8266_and_ESP32_OLED_driver_for_SSD1306_displays
#include <WiFi.h>
#include <WiFiMulti.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>  // Keith O'Leary
#include <elapsedMillis.h>
#include <LinkedList.h>

WiFiMulti WiFiMulti;

const char* ssid     = "New Home";                          //WiFi Name
// For wifi password, please create a new file called WifiPassword.h
// with a line like this: 
// const char* password = "MyWifiPassword";

#include "./WifiPassword.h"
 
/*
   MQTT server details
*/
const char* mqttServer = "192.168.1.5";
const int mqttPort = 1883;
const char* mqttUser = "";
const char* mqttPassword = "";
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];

WiFiClient espClient;
PubSubClient client(espClient);

#define NTP_OFFSET  3600 // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "pool.ntp.org"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

SSD1306 display(0x3c, 32, 33); // instance for the OLED. Addr, SDA, SCL

#include "./Devices.h" // valves, pumps, thermostat classes (use the constants defines above)
#include "./HeatingZone.h"
#include "./HeatingSystem.h"

////////////////////////////////////////////////////
//   CONFIGURATION BLOCK

// Configure/reorder your pinning as you like (This my wiring on an ESP32);

#define BOILER_PIN     16  // output to a Relay for Boiler -- Relay Channel 2
#define FU_PUMP_PIN    17  // output to a Relay that switches the Pump -- Relay Channel 4

// Definitions for the pins for my zones... Could be improved/not hard-coded, should be part of the Zone objects! Sorry about that!
#define MILL_V 13  // Zone 1: output to a Relay that controls the Valve(s) // Note - zones 1 and 2 are swapped on my system
#define MILL_MICROSWITCH 23 
#define ENG_RM_VALVE  12  // Zone 2: output to a Relay that controls the Valve(s)
#define ENG_RM_MICROSWITCH 22 
#define GRANARY_VALVE   14  // Zone 3: output to a Relay that controls the Valve(s) -- Relay Channel 1
#define GRANARY_MICROSWITCH 26
#define DHW_VALVE   15 // DHW: output to a Relay that controls the Valve(s) -- Relay Channel 3
#define DHW_MICROSWITCH 27 // for some reason, pin 35 doesn't seem to work for this purpose - it needed external pullup!

//#define MILL_T  17  // Zone 1; input wired to the thermostat in the living room
//#define ENG_RM_THERMO 27  // Zone 2; input wired to the thermostat in engine room
//#define GRANARY_THERMO  18 // Zone 3; input wired to the thermostat in the granary
#define DHW_THERMO      21 // DHW Thermostat

//#define HEATING_LED    20 // On when heating, Alternates during cooldown, is Off in idle mode
#define INDICATION_LED 25 // Alternates the on board LED to indicate board runs; can be easily removed to free an extra IO pin!!

// END CONFIGURATION BLOCK
//////////////////////////////////////////////////


// Some fixed devices:
LED           iLED(INDICATION_LED, "Indicator LED"); // can be removed if you run out of IO's
//LED           hLED(HEATING_LED, "Heating LED");
Manipulator   CV(BOILER_PIN, "Boiler");
Pump          FUPump(FU_PUMP_PIN, "Pump");

// The main controller
HeatingSystem heatingSystem(BOILER_PIN, FU_PUMP_PIN);

void printConfiguration() {
  clearDisplay();
  iLED.PrintState();
  //hLED.PrintState();
  printOLED(CV.PrintState());
  printOLED(FUPump.PrintState());
  printOLED(heatingSystem.PrintState());
  
}

bool connectToWifi(bool forceReconnect) {
  if (!forceReconnect && WiFi.status() == WL_CONNECTED) {
    return true;
  }
  clearDisplay();
  if (forceReconnect) {
    printOLED("Force WiFi");
    WiFi.disconnect();
  }
  
  int retries = 0;
  while (WiFiMulti.run() != WL_CONNECTED && retries < 5)
  {
    //WiFi.disconnect();
    printOLED("Connecting to WiFi");
    Serial.println("Connecting to WiFi");
    WiFi.begin(ssid, password);
    delay(4000);  
    retries ++;
  }
  clearDisplay();
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  printOLED("WiFi connected.");
  printOLED("IP address: ");
  printOLED(WiFi.localIP().toString());
  
  return true;
}

bool mqttConnect(bool forceReconnect) {
  if (!forceReconnect && client.connected()) return true;
  // MQTT
  client.setServer(mqttServer, mqttPort);

  int retries = 0;
  clearDisplay();
  printTimeStamp();
  while ((forceReconnect && retries < 10) || (!client.connected() && retries < 10)) {
    printOLED("Connecting to MQTT...");
    
    client.setKeepAlive( 90 ); // setting keep alive to 90 seconds
    if (client.connect("Heating Controller", mqttUser, mqttPassword )) {
      client.publish("esp/test", "Setup bonjour");
      // Now subscribe to all the topics we need
      heatingSystem.SubscribeToAllTopics();
      client.setCallback(callback);
      printOLED("Connected to MQTT");
      return (true);
    }
    retries++;
    delay(500);
  }
  clearDisplay();
  printOLED("MQTT connect Failed");
  printOLED(String(client.state()));
  return false;
}

void setup()
{
  // initializations
  Serial.begin(115200);
  display.init(); // initialise the OLED
  display.setFont(ArialMT_Plain_10); // does what is says
  // Set the origin of text to top left
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  WiFiMulti.addAP(ssid, password);


  if (!connectToWifi(false)) {
    printOLED("Can't connect to WiFi!");
    delay(5000);
    ESP.restart();
  }
  
  timeClient.begin();

  // 
  // OTA Stuff
  //
  // Port defaults to 3232
  ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("HeatingControl");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with its md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  
  HeatingZone *mill = new HeatingZone("Mill",  new Valve(MILL_V, MILL_MICROSWITCH, "V-M"),  NULL);
  HeatingZone *engr = new HeatingZone("EngR", new Valve(ENG_RM_VALVE, ENG_RM_MICROSWITCH, "V-E"), NULL);
  HeatingZone *gran = new HeatingZone("Gran",  new Valve(GRANARY_VALVE, GRANARY_MICROSWITCH, "V-G"), NULL);
  HeatingZone *dhw = new HeatingZone("DHW", new Valve(DHW_VALVE, DHW_MICROSWITCH, "V-D"), new Thermostat(DHW_THERMO, "T-D"));
  heatingSystem.AddZone(mill);
  heatingSystem.AddZone(engr);
  heatingSystem.AddZone(gran);
  heatingSystem.AddZone(dhw);

  mqttConnect(false);
  
  printConfiguration();
  //wdt_enable(WDTO_1S);  // Watchdog: reset board after one second, if no "pat the dog" received
}

elapsedMillis sinceUpdate;
elapsedMillis sinceSuccessfulMqtt;
elapsedMillis sinceReconnect;

void loop () {

  if (sincePrintOLED > 100 && !flushed) {
    flushDisplay();
  }
  
  bool forceReconnect = !client.loop();
  if (sinceReconnect > 3600000) { // 1hr
    client.disconnect();
    sinceReconnect = 0;
    forceReconnect = true;
  }
  
  if (!connectToWifi(forceReconnect)) {
    printOLED("Can't connect to WiFi!");
    delay(20000);
    ESP.restart();
  }
  
  if (mqttConnect(forceReconnect)) {
    sinceSuccessfulMqtt = 0;
  }

  if (sinceSuccessfulMqtt > 300000) {
    printOLED("Can't connect to MQTT!");
    
    delay(20000);
    ESP.restart();
  }

  ArduinoOTA.handle();
  timeClient.update(); 

  // Small delay has been suggested to improve stability of MQTT connection. Doesn't seem to help, so remove it for now
  //delay(100);
    
  if (sinceUpdate > 1000) {
    
    //Serial.println("Updating");
    // Use Indication LED to show board is alive
    iLED.Alternate();
    heatingSystem.Update();

    // Reset the WatchDog timer (pat the dog)
    //wdt_reset();

    sinceUpdate = 0;
  }
}

//MQTT Callback
void callback(char* topic, byte* payload, unsigned int length) {
  heatingSystem.HandleMqtt(topic, payload, length);
}

void printTimeStamp() {
  unsigned long seconds = millis() / (unsigned long)1000;
  unsigned long minutes, hours, days;
  minutes = seconds / 60L;
  seconds %= 60L;
  hours = minutes / 60L;
  minutes %= 60L;
  days = hours / 24L;
  hours %= 24L;
  char time[30];
  sprintf(time, "%02d:%02d:%02d:%02d", (int)days, (int)hours, (int)minutes, (int)seconds);

  printOLED(time);
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();
  printOLED(formattedTime);
}
