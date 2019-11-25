/**
 * Secure Write Example code for InfluxDBClient library for Arduino
 * Demonstrates connection to InfluxDB 2 Cloud  https://cloud2.influxdata.com/
 * Data can be immediately seen in a InfluxDB 2 Cloud UI
 */

#include "InfluxDbClient.h"
#include "InfluxDbCloud.h"

#if defined(ESP32) 
# include <WiFiMulti.h>
WiFiMulti wifiMulti;
#elif defined(ESP8266)   
# include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#endif

// WiFi AP SSID
#define WIFI_SSID "SSID"
// WiFi password
#define WIFI_PASSWORD "PASSWORD"

// InfluxDB v2 server or cloud url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com
#define INFLUXDB_URL "server-url"
// InfluxDB v2 server or cloud API authentication token
#define INFLUXDB_TOKEN "server token"
// InfluxDB v2 organization id 
#define INFLUXDB_ORG "org id"
// InfluxDB v2 bucket name
#define INFLUXDB_BUCKET "bucket name"

// Timezone string accoring to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Some values:
// Pacific Time: PST8PDT
// Eastern: EST5EDT
// Japanesse: JST-9
// Central Europe: CET-1CEST,M3.5.0,M10.5.0/3
#define TZ_INFO  "CET-1CEST,M3.5.0,M10.5.0/3"


// Single client instance
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Data point
Point sensor("wifi_status");

void setup() {
  Serial.begin(115200);


  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.println();
  Serial.print("Connecting to wifi ");
  int i = 0;
  while(wifiMulti.run() != WL_CONNECTED && i<100) {
    Serial.print(".");
    delay(100);
    i++;
  }
  Serial.println();

  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Wifi connection failed");
    // Wait forever
    while(1) delay(100);
  }
  
#if defined(ESP32) 
  String deviceName = "ESP32";
#elif defined(ESP8266) 
  String deviceName = "ESP8266";
#endif 
  // Add tags 
  sensor.addTag("device", deviceName);  
  sensor.addTag("SSID",WiFi.SSID());
  // Synchronize time with NTP server
  // Accurate time is neccessary for certificate validaton
  // Synchronize time as we are in UTC 
  configTime(0, 0 , "pool.ntp.org", "time.nis.gov");
  // Set correct timezone 
  setenv("TZ",TZ_INFO, 1);
 
  // Wait till time is synced
  Serial.print("Waiting till time is synced ");
  i = 0;
  while(time(nullptr) < 1000000000ul && i<100) {
    Serial.print(".");
    delay(100);
    i++;
  }
  Serial.println();

  
  Serial.println("Validating connection to server");
  // Check server connection
  if(!client.validateConnection()) {
    Serial.print("Connection failed: ");
    Serial.println(client.getLastErrorMessage());
    // Wait forver
    while(1) delay(100);
  }
}

void loop() {
  sensor.addField("rssi",  WiFi.RSSI());

  // print what we are exactly writing
  Serial.print("Writing: ");
  Serial.println(sensor.toLineProtocol());
  // Write point
  if(!client.writePoint(sensor)) {
    Serial.print("Write failed: ");
    Serial.println(client.getLastErrorMessage());
  }
  
  // Clear fields for next usage. Tags remain the same.
  sensor.clearFields();

  delay(10000);
}