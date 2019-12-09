/**
 * Query Example code for InfluxDBClient library for Arduino
 * Enter WiFi and InfluxDB parameters below
 *
 * Demonstrates connection to any InfluxDB instance accesible via:
 *  - unsecured http://...
 *  - secure https://... (appropriate certificate is required)
 *  - InfluxDB 2 Cloud at https://cloud2.influxdata.com/ (certificate is preconfigured)
 * This example demonstrates querying basic statistic parameters of WiFi signal level measured and stored in BasicWrite and SecureWrite examples
 **/

#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "ESP32"
#elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// WiFi AP SSID
#define WIFI_SSID "SSID"
// WiFi password
#define WIFI_PASSWORD "PASSWORD"
// InfluxDB v2 server url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "server-url"
// InfluxDB v2 server or cloud API authentication token (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
#define INFLUXDB_TOKEN "server token"
// InfluxDB v2 organization name or id (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
#define INFLUXDB_ORG "org name/id"
// InfluxDB v2 bucket name (Use: InfluxDB UI -> Load Data -> Buckets)
#define INFLUXDB_BUCKET "bucket name"

// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Examples:
//  Pacific Time: "PST8PDT"
//  Eastern: "EST5EDT"
//  Japanesse: "JST-9"
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO  "CET-1CEST,M3.5.0,M10.5.0/3"

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

void timeSync() {
  // Synchronize UTC time with NTP servers
  // Accurate time is necessary for certificate validaton 
  configTime(0, 0 , "pool.ntp.org", "time.nis.gov");
  // Set timezone
  setenv("TZ",TZ_INFO, 1);

  // Wait till time is synced
  Serial.print("Syncing time");
  int i = 0;
  while (time(nullptr) < 1000000000ul && i<100) {
    Serial.print(".");
    delay(100);
    i++;
  }
  Serial.println();

  // Show time
  time_t tnow = time(nullptr);
  Serial.print( "Synchronized time: ");
  Serial.println(String(ctime(&tnow)));
}

void setup() {
  Serial.begin(115200);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Sync time for certificate validation
  timeSync();

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println( client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop() {
  printQuery("max");
  printQuery("mean");
  printQuery("min");
  
  //Wait 10s
  Serial.println("Wait 10s");
  delay(10000);
}

void printQuery(String selectorFunction) {
    // Construct a Flux query
    // Query will find RSSI for each connected WiFi network with this device computed by given selector function
    String query = "from(bucket: \""  INFLUXDB_BUCKET  "\") |> range(start: -24h) |> filter(fn: (r) => r._measurement == \"wifi_status\" and r._field == \"rssi\"";
    query += "and r.device == \"" DEVICE "\")";
    query +="|> " + selectorFunction + "()";

    Serial.println("Quering with:");
    Serial.println(query);
    String resultSet = client.query(query);
    Serial.println("Query raw result:");
    Serial.println(resultSet);
    
    // Result set in CSV table, with header 
    // First parse retuned string to lines
    int linesCount;
    String *lines = getLines(resultSet, linesCount);
    // If result contains some lines it has some data
    if(linesCount > 0) {
        // First parse header line to arrays of columns
        int columnsCount;
        String *columnNames = getColumns(lines[0], columnsCount);
        // Find index of column named '_value', which contains value 
        int valueColumnIndex = findItem(columnNames, columnsCount, "_value");
        // Find index of column named 'SSID', which is our tag with WiFi name 
        int ssidColumnIndex = findItem(columnNames, columnsCount, "SSID");
        // We have all information. Save RAM by deleting array with column names
        delete [] columnNames;
        // If result set contains all we need
        if(valueColumnIndex != -1 && ssidColumnIndex != -1) {
            // Iterate over lines with data to get values
            Serial.print(selectorFunction);
            Serial.println("(RSSI):");
            for(int i=1;i<linesCount;i++) {
                // Parse next line into values
                String *columnValues = getColumns(lines[i], columnsCount);
                Serial.print("  ");
                Serial.print(columnValues[ssidColumnIndex]);
                Serial.print(":");
                Serial.println(columnValues[valueColumnIndex]);
                delete [] columnValues;
            }
        }
    }
    delete [] lines;
}

// Finds index of item in array
int findItem(String *array, int len, String item) {
  for(int i=0;i<len;i++) {
      if(item == array[i]) {
          return i;
      }
  }
  return -1;
}

int countParts(String &str, char separator) {
  int lines = 0;
  int i,from = 0;
  while((i = str.indexOf(separator, from)) >= 0) {
    ++lines;
    from = i+1;
  }
  // try last part
  if(from < str.length()) {
      String s = str.substring(from);
      s.trim();
      if(s.length()>0) {
        ++lines;
      }
  }
  return lines;
}

// Utitility function for splitting string str by separator char
String *getParts(String &str, char separator, int &count) {
//Serial.printf("Parsing for %c:\n'%s'\n", separator, str.c_str());
  count = countParts(str, separator);
  //Serial.printf(" Count %d:\n", count);
  String *ret = new String[count];
  int i,from = 0,p=0;
  while((i = str.indexOf(separator, from)) >= 0) {
    ret[p] = str.substring(from,i);
    ret[p++].trim();
    from = i+1;
  }
  // try the last part
  if(from < str.length()) {
      String s = str.substring(from);
      s.trim();
     // Serial.printf("  Last part: '%s',len: %d\n",s.c_str(), s.length());
      if(s.length() > 0) {
        ret[p] = s;
      }
  }
  return ret;
}


String *getLines(String &str, int &count) {
  return getParts(str, '\n', count);
}

String *getColumns(String &str, int &count) {
  return getParts(str, ',', count);
}