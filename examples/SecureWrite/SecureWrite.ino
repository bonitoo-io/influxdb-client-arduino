/**
 * Secure Write Example code for InfluxDBClient library for Arduino
 * Demonstrates connection to InfluxDB 2 Cloud  https://cloud2.influxdata.com/
 * Data can be immediately seen in a InfluxDB 2 Cloud UI
 */

#include "InfluxDbClient.h"
#if defined(ESP32) 
   #include <WiFi.h>
#elif defined(ESP8266) 
  #include <ESP8266WiFi.h>
#endif


// InfluxDB v2 server or cloud url
#define INFLUXDB_URL "server-url"
// InfluxDB v2 server or cloud API authentication token
#define INFLUXDB_TOKEN "server token"
// InfluxDB v2 organization id
#define INFLUXDB_ORG "org id"
// InfluxDB v2 bucket name
#define INFLUXDB_BUCKET "bucket name"

// Timezone offset from UTC in sec 
// 3600 for Central Europe
// -4*3600 for Eastern
// -6*3600 for Pacific Time
#define NTP_TZ_OFFSET 3600
// Day light saving offset in sec (0 for standard time, 3600 da
#define NTP_DST_OFFSET 0

// Certificate Authority of InfluxData Cloud 2 servers
const char InfluxDataCloud2CACert[] PROGMEM =  R"EOF( 
-----BEGIN CERTIFICATE-----
MIIGEzCCA/ugAwIBAgIQfVtRJrR2uhHbdBYLvFMNpzANBgkqhkiG9w0BAQwFADCB
iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl
cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV
BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTgx
MTAyMDAwMDAwWhcNMzAxMjMxMjM1OTU5WjCBjzELMAkGA1UEBhMCR0IxGzAZBgNV
BAgTEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4GA1UEBxMHU2FsZm9yZDEYMBYGA1UE
ChMPU2VjdGlnbyBMaW1pdGVkMTcwNQYDVQQDEy5TZWN0aWdvIFJTQSBEb21haW4g
VmFsaWRhdGlvbiBTZWN1cmUgU2VydmVyIENBMIIBIjANBgkqhkiG9w0BAQEFAAOC
AQ8AMIIBCgKCAQEA1nMz1tc8INAA0hdFuNY+B6I/x0HuMjDJsGz99J/LEpgPLT+N
TQEMgg8Xf2Iu6bhIefsWg06t1zIlk7cHv7lQP6lMw0Aq6Tn/2YHKHxYyQdqAJrkj
eocgHuP/IJo8lURvh3UGkEC0MpMWCRAIIz7S3YcPb11RFGoKacVPAXJpz9OTTG0E
oKMbgn6xmrntxZ7FN3ifmgg0+1YuWMQJDgZkW7w33PGfKGioVrCSo1yfu4iYCBsk
Haswha6vsC6eep3BwEIc4gLw6uBK0u+QDrTBQBbwb4VCSmT3pDCg/r8uoydajotY
uK3DGReEY+1vVv2Dy2A0xHS+5p3b4eTlygxfFQIDAQABo4IBbjCCAWowHwYDVR0j
BBgwFoAUU3m/WqorSs9UgOHYm8Cd8rIDZsswHQYDVR0OBBYEFI2MXsRUrYrhd+mb
+ZsF4bgBjWHhMA4GA1UdDwEB/wQEAwIBhjASBgNVHRMBAf8ECDAGAQH/AgEAMB0G
A1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAbBgNVHSAEFDASMAYGBFUdIAAw
CAYGZ4EMAQIBMFAGA1UdHwRJMEcwRaBDoEGGP2h0dHA6Ly9jcmwudXNlcnRydXN0
LmNvbS9VU0VSVHJ1c3RSU0FDZXJ0aWZpY2F0aW9uQXV0aG9yaXR5LmNybDB2Bggr
BgEFBQcBAQRqMGgwPwYIKwYBBQUHMAKGM2h0dHA6Ly9jcnQudXNlcnRydXN0LmNv
bS9VU0VSVHJ1c3RSU0FBZGRUcnVzdENBLmNydDAlBggrBgEFBQcwAYYZaHR0cDov
L29jc3AudXNlcnRydXN0LmNvbTANBgkqhkiG9w0BAQwFAAOCAgEAMr9hvQ5Iw0/H
ukdN+Jx4GQHcEx2Ab/zDcLRSmjEzmldS+zGea6TvVKqJjUAXaPgREHzSyrHxVYbH
7rM2kYb2OVG/Rr8PoLq0935JxCo2F57kaDl6r5ROVm+yezu/Coa9zcV3HAO4OLGi
H19+24rcRki2aArPsrW04jTkZ6k4Zgle0rj8nSg6F0AnwnJOKf0hPHzPE/uWLMUx
RP0T7dWbqWlod3zu4f+k+TY4CFM5ooQ0nBnzvg6s1SQ36yOoeNDT5++SR2RiOSLv
xvcRviKFxmZEJCaOEDKNyJOuB56DPi/Z+fVGjmO+wea03KbNIaiGCpXZLoUmGv38
sbZXQm2V0TP2ORQGgkE49Y9Y3IBbpNV9lXj9p5v//cWoaasm56ekBYdbqbe4oyAL
l6lFhd2zi+WJN44pDfwGF/Y4QA5C5BIG+3vzxhFoYt/jmPQT2BVPi7Fp2RBgvGQq
6jG35LWjOhSbJuMLe/0CjraZwTiXWTb2qHSihrZe68Zk6s+go/lunrotEbaGmAhY
LcmsJWTyXnW0OMGuf1pGg+pRyrbxmRE1a6Vqe8YAsOf4vmSyrcjC8azjUeqkk+B5
yOGBQMkKW+ESPMFgKuOXwIlCypTPRpgSabuY0MLTDXJLR27lk8QyKGOHQ+SwMj4K
00u/I5sUKUErmgQfky3xxzlIPK1aEn8=
-----END CERTIFICATE-----
)EOF";

// Single client instance
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDataCloud2CACert);

// Data point
Point sensor("wifi_status");

void setup() {
  Serial.begin(115200);


  WiFi.mode(WIFI_STA);
  //WiFi.begin("SSID", "PASSWORD");
  WiFi.begin("666G", "andromeda");

  int i = 0;
  while ((WiFi.status() != WL_CONNECTED) && (i<100)) { 
    delay(100);
    i++;
  }

  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Wifi connection failed");
    while(1) delay(100);
  }
  
#if defined(ESP32) 
  String deviceName = "ESP32";
#elif defined(ESP8266) 
  String deviceName = "ESP8266";
#endif 
  sensor.addTag("device", deviceName);  
  sensor.addTag("SSID",WiFi.SSID());
  // Synchronize time with NTP server
  // Accurate time is neccessary for certificate validaton
  configTime(NTP_DST_OFFSET, NTP_TZ_OFFSET,  "pool.ntp.org", "time.nist.gov");
  
  // Check server connection
  if(!client.validateConnection()) {
    Serial.println("Connection failed: ");
    while(1) delay(100);
  }
}

void loop() {
  sensor.addField("rssi",  WiFi.RSSI());

  // Write point
  bool writeOk = client.writePoint(sensor);
  
  // Clear fields for next usage. Tags remain the same.
  sensor.clearFields();

  delay(10000);
}
