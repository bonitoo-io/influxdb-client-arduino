#define INFLUXDB_CLIENT_TESTING
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#if defined(ESP32) 
   #include <WiFiMulti.h>
WiFiMulti wifiMulti;
String chipId = String((unsigned long)ESP.getEfuseMac());
String deviceName = "ESP32";
#elif defined(ESP8266) 
  #include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
String chipId = String(ESP.getChipId());
String deviceName = "ESP8266";
#endif

#define INFLUXDB_CLIENT_TESTING_URL "http://192.168.1.73:999"
#define INFLUXDB_CLIENT_TESTING_ORG "my-org"
#define INFLUXDB_CLIENT_TESTING_BUC "my-bucket"
#define INFLUXDB_CLIENT_TESTING_TOK "1234567890"


int failures = 0;

void setup() {
  Serial.begin(115200);

  //Serial.setDebugOutput(true);
  randomSeed(123);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  wifiMulti.addAP("bonitoo.io", "change1t");
  wifiMulti.addAP("666G", "andromeda");
  wifiMulti.addAP("667G", "andromeda");
  
  Serial.println();
  
  initInet();
  
}


#define TEST_INIT(name) int temp = failures; do { Serial.println(name)
#define TEST_END(name)  } while(0); Serial.printf("%s %s\n",name,failures == temp?"SUCCEEDED":"FAILED");
#define TEST_ASSERT(a) if(testAssert(__LINE__, (a))) break;


void loop() {
  testPoint();
  testBufferOverwriteBatchsize1();
  testRetryOnFailedConnection();
  
  Serial.printf("Test %s\n", failures?"FAILED":"SUCCEEDED");
  while(1) delay(1000);
}

void testPoint() {
   TEST_INIT("testPoint");
   
   Point p("test");
   TEST_ASSERT(!p.hasTags());
   TEST_ASSERT(!p.hasFields());
   p.addTag("tag1","tagvalue");
   TEST_ASSERT(p.hasTags());
   TEST_ASSERT(!p.hasFields());
   p.addField("field1",23);
   TEST_ASSERT(p.hasFields());

   TEST_END("testPoint");
}

void testRetryOnFailedConnection() {
  TEST_INIT("testRetryOnFailedConnection");
  
  InfluxDBClient clientOk(INFLUXDB_CLIENT_TESTING_URL,INFLUXDB_CLIENT_TESTING_ORG,INFLUXDB_CLIENT_TESTING_BUC,INFLUXDB_CLIENT_TESTING_TOK);
  clientOk.setWriteOptions(WritePrecision::NoTime, 1, 5);
  if(clientOk.validateConnection()) {
    Serial.println("Waiting server is down");
    waitServer(clientOk, false);
  }
  TEST_ASSERT(!clientOk.validateConnection());
  Point *p = createPoint("test1");
  TEST_ASSERT(!clientOk.writePoint(*p));
  delete p;
  p = createPoint("test1");
  TEST_ASSERT(!clientOk.writePoint(*p));
  delete p;
  Serial.println("Start server!");
  waitServer(clientOk, true);
  TEST_ASSERT(clientOk.validateConnection());
  p = createPoint("test1");
  TEST_ASSERT(clientOk.writePoint(*p));
  delete p;
  TEST_ASSERT(clientOk.isBufferEmpty());
  String query = "select";
  String q = clientOk.queryString(query);
  TEST_ASSERT(countLines(q) == 4); //3 points+header
  
  waitServer(clientOk, true);
  TEST_ASSERT(deleteAll(clientOk));
  TEST_END("testRetryOnFailedConnection");
}

void testBufferOverwriteBatchsize1() {
  TEST_INIT("testBufferOverwriteBatchsize1");
  InfluxDBClient client("http://127.0.0.1:999", INFLUXDB_CLIENT_TESTING_ORG,INFLUXDB_CLIENT_TESTING_BUC,INFLUXDB_CLIENT_TESTING_TOK);
  client.setWriteOptions(WritePrecision::NoTime, 1, 5);

  TEST_ASSERT(!client.validateConnection());
  for(int i=0;i<6;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    TEST_ASSERT(!client.writePoint(*p));
    delete p;
  }
  TEST_ASSERT(client.isBufferFull());
  TEST_ASSERT(client.getBuffer()[0].indexOf("index=5i")>0);
  
  client.setServerUrl(INFLUXDB_CLIENT_TESTING_URL);
  waitServer(client, true);
  
  Point *p = createPoint("test1");
  p->addField("index", 6);
  TEST_ASSERT(client.writePoint(*p));
  TEST_ASSERT(client.isBufferEmpty());

  String query = "select";
  String q = client.queryString(query);
  TEST_ASSERT(countLines(q) == 6); //5 points+header
  String *lines = getLines(q);
  TEST_ASSERT(lines[1].indexOf(",2")>0);
  TEST_ASSERT(lines[2].indexOf(",3")>0);
  TEST_ASSERT(lines[3].indexOf(",4")>0);
  TEST_ASSERT(lines[4].indexOf(",5")>0);
  TEST_ASSERT(lines[5].indexOf(",6")>0);
  
  deleteAll(client);
  TEST_END("testBufferOverwriteBatchsize1");
}

void testBufferOverwriteBatchsize4() {
  TEST_INIT("testBufferOverwriteBatchsize1");
  InfluxDBClient client("http://127.0.0.1:999", INFLUXDB_CLIENT_TESTING_ORG,INFLUXDB_CLIENT_TESTING_BUC,INFLUXDB_CLIENT_TESTING_TOK);
  client.setWriteOptions(WritePrecision::NoTime, 5, 30);

  TEST_ASSERT(!client.validateConnection());
  for(int i=0;i<6;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    TEST_ASSERT(!client.writePoint(*p));
    delete p;
  }
  TEST_ASSERT(client.isBufferFull());
  TEST_ASSERT(client.getBuffer()[0].indexOf("index=5i")>0);
  
  client.setServerUrl(INFLUXDB_CLIENT_TESTING_URL);
  waitServer(client, true);
  
  Point *p = createPoint("test1");
  p->addField("index", 6);
  TEST_ASSERT(client.writePoint(*p));
  TEST_ASSERT(client.isBufferEmpty());

  String query = "select";
  String q = client.queryString(query);
  TEST_ASSERT(countLines(q) == 6); //5 points+header
  String *lines = getLines(q);
  TEST_ASSERT(lines[1].indexOf(",2")>0);
  TEST_ASSERT(lines[2].indexOf(",3")>0);
  TEST_ASSERT(lines[3].indexOf(",4")>0);
  TEST_ASSERT(lines[4].indexOf(",5")>0);
  TEST_ASSERT(lines[5].indexOf(",6")>0);
  
  deleteAll(client);
  TEST_END("testBufferOverwriteBatchsize1");
}

bool deleteAll(InfluxDBClient &client) {
  String deletePoint = "db,direction=delete-all a=1";
  return client.writeRecord(deletePoint);
}

int countLines(String &str) {
  int lines = 0;
  int i,from = 0;
  while((i = str.indexOf('\n', from)) >= 0) {
    lines++;
    from = i+1;
  }
  return lines;
}

String *getLines(String &str) {
  int size = countLines(str);
  String *ret = new String[size];
  int i,from = 0,p=0;
  while((i = str.indexOf('\n', from)) >= 0) {
    ret[p++] = str.substring(from,i);
    from = i+1;
  }
  return ret;
}

bool testAssert(int line, bool state) {
  if(!state) {
    ++failures;
    Serial.printf("Assert failure: Line %d\n", line);
    return true;
  }
  return false;
}

// Waits for server in desired state (up - true, down - false)
bool waitServer(InfluxDBClient &client, bool state) {
  int c = 0;
  bool res = false;
  while((res = client.validateConnection()) != state && c++ < 30) {
      Serial.printf("  Server is not %s\n", state?"up":"down");
      delay(1000);
  }
  return res == state;
}

Point *createPoint(String measurement) {
  Point *point = new Point(measurement);
  point->addTag("SSID",WiFi.SSID());
  point->addTag("device_name", deviceName);
  point->addTag("device_id", chipId);
  point->addField("temperature",  random(-20,40)*1.1f);
  point->addField("humidity",  random(10,90));
  point->addField("code",  random(10,90));
  point->addField("door",  random(0,10)>5);
  point->addField("status",  random(0,10)>5?"ok":"failed");
  return point;
}

void initInet() {
  int i = 0;
  Serial.print("Connecting to wifi ");
  while ((wifiMulti.run(10000) != WL_CONNECTED) && (i<100)) { 
    Serial.print(".");
    delay(300);
    i++;
  }
  Serial.println();
  bool wifiOk = WiFi.status() == WL_CONNECTED;
  if(!wifiOk) {
    Serial.println("Wifi connection failed");
    while(1) delay(100);
  } else {
    Serial.printf("Connected to: %s\n",WiFi.SSID().c_str());

    configTime(0, 0, "pool.ntp.org", "0.cz.pool.ntp.org", "1.cz.pool.ntp.org");
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    
    // Wait till time is synced
    Serial.print("Waiting till time is synced ");
    i = 0;
    while(time(nullptr) < 1000000000ul && i<100) {
      Serial.print(".");
      delay(100);
      i++;
    }
    Serial.println("");
    printTime();
  }
}

void printTime() {
  time_t now = time(nullptr);
  struct tm *tmstruct = localtime(&now);
  Serial.printf("Local: %d.%d.%d %02d:%02d\n", tmstruct->tm_mday,( tmstruct->tm_mon)+1, (tmstruct->tm_year)+1900,tmstruct->tm_hour , tmstruct->tm_min);
  tmstruct = gmtime(&now);
  Serial.printf("GMT:   %d.%d.%d %02d:%02d\n", tmstruct->tm_mday,( tmstruct->tm_mon)+1, (tmstruct->tm_year)+1900,tmstruct->tm_hour , tmstruct->tm_min);
}