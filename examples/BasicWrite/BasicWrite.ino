/**
 * Basic Write Example code for InfluxDBClient library for Arduino
 * Data can be immediately seen in a InfluxDB 2 UI
 * Enter WiFi and InfluxDB parameters below
 *
 * This example supports only InfluxDB running from unsecure (http://...)
 * For secure (https://...) or Influx Cloud 2 use SecureWrite example
 */

#include "InfluxDbClient.h"
#include "InfluxDbCloud.h"

#if defined(ESP32) 
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "ESP32"
#elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
#endif

// WiFi AP SSID
#define WIFI_SSID "ssid"
// WiFi password
#define WIFI_PASSWORD "password"
// InfluxDB v2 server url, e.g. http://192.168.1.48:9999 (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "influxdb-url"
// InfluxDB v2 server or cloud API authentication token (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
#define INFLUXDB_TOKEN "toked-id"
// InfluxDB v2 organization id (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
#define INFLUXDB_ORG "org"
// InfluxDB v2 bucket name (Use: InfluxDB UI -> Load Data -> Buckets)
#define INFLUXDB_BUCKET "bucket"

// Single InfluxDB instance
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
// Data point
Point sensor("wifi_status");

void setup() {
  Serial.begin(115200);

  // Connect WiFi
  Serial.println("Connecting to WiFi ");
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while(wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Add constant tags - only once
  sensor.addTag("device", DEVICE);
  sensor.addTag("SSID", WiFi.SSID());
}

void loop() {
  // Store measured value into point
  sensor.clearFields();
  sensor.addField("rssi", WiFi.RSSI());
  Serial.print("Writing: ");
  Serial.println(sensor.toLineProtocol());

  // Write point
  if(!client.writePoint(sensor)) {
    Serial.print("Write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  //Wait 10s
  delay(10000);
}
