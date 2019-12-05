/**
 *  E2E tests for InfluxDBClient.
 *  For compiling in VSCode add path to workspace ("${workspaceFolder}\\**")
 * 
 */

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

#define INFLUXDB_CLIENT_TESTING_URL "http://192.168.88.36:999"
#define INFLUXDB_CLIENT_TESTING_ORG "my-org"
#define INFLUXDB_CLIENT_TESTING_BUC "my-bucket"
#define INFLUXDB_CLIENT_TESTING_TOK "1234567890"
#define INFLUXDB_CLIENT_TESTING_SSID "SSID"
#define INFLUXDB_CLIENT_TESTING_PASS "password"
#define INFLUXDB_CLIENT_TESTING_BAD_URL "http://127.0.0.1:999"

int failures = 0;

#include "TestSupport.h"

void setup() {
  Serial.begin(115200);

  //Serial.setDebugOutput(true);
  randomSeed(123);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  wifiMulti.addAP(INFLUXDB_CLIENT_TESTING_SSID,INFLUXDB_CLIENT_TESTING_PASS);


  Serial.println();
  
  initInet();

  //tests 
  testPoint();
  testRetryOnFailedConnection();
  testBufferOverwriteBatchsize1();
  testBufferOverwriteBatchsize5();
  testServerTempDownBatchsize5();
  testRetriesOnServerOverload();
  
  Serial.printf("Test %s\n", failures?"FAILED":"SUCCEEDED");
}

void loop() {
  delay(1000);
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
  p.addField("field2",true);
  p.addField("field3",1.123);
  p.addField("field4","texttest");
  String line = p.toLineProtocol();
  String testLine = "test,tag1=tagvalue field1=23i,field2=true,field3=1.12,field4=\"texttest\"";
  TEST_ASSERTM(line == testLine,line);
  
  time_t now = time(nullptr);
  p.setTime(now);   
  String testLineTime = testLine + " " + now;
  line = p.toLineProtocol();
  TEST_ASSERTM(line == testLineTime,line);
  
  now += 10;
  String nowStr(now);
  p.setTime(nowStr);
  testLineTime = testLine + " " + nowStr;
  line = p.toLineProtocol();
  TEST_ASSERTM(line == testLineTime,line);

  p.setTime(WritePrecision::S);
  line = p.toLineProtocol();
  int partsCount; 
  String *parts = getParts(line, ' ', partsCount);
  TEST_ASSERTM(partsCount == 3, String("3 != ")  + partsCount);
  TEST_ASSERT(parts[2].length() == nowStr.length());
  delete [] parts;

  p.setTime(WritePrecision::MS);
  line = p.toLineProtocol();
  parts = getParts(line, ' ', partsCount);
  TEST_ASSERT(partsCount == 3);
  TEST_ASSERT(parts[2].length() == nowStr.length()+3);
  delete [] parts;

  p.setTime(WritePrecision::US);
  line = p.toLineProtocol();
  parts = getParts(line, ' ', partsCount);
  TEST_ASSERT(partsCount == 3);
  TEST_ASSERT(parts[2].length() == nowStr.length()+6);
  delete [] parts;

   p.setTime(WritePrecision::NS);
  line = p.toLineProtocol();
  parts = getParts(line, ' ', partsCount);
  TEST_ASSERT(partsCount == 3);
  TEST_ASSERT(parts[2].length() == nowStr.length()+9);
  delete [] parts;


  TEST_END();
}

void testRetryOnFailedConnection() {
  TEST_INIT("testRetryOnFailedConnection");
  
  InfluxDBClient clientOk(INFLUXDB_CLIENT_TESTING_URL,INFLUXDB_CLIENT_TESTING_ORG,INFLUXDB_CLIENT_TESTING_BUC,INFLUXDB_CLIENT_TESTING_TOK);
  clientOk.setWriteOptions(WritePrecision::NoTime, 1, 5);
  waitServer(clientOk, false);
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
  
  TEST_END();
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);
}

void testBufferOverwriteBatchsize1() {
  TEST_INIT("testBufferOverwriteBatchsize1");
  InfluxDBClient client(INFLUXDB_CLIENT_TESTING_BAD_URL, INFLUXDB_CLIENT_TESTING_ORG,INFLUXDB_CLIENT_TESTING_BUC,INFLUXDB_CLIENT_TESTING_TOK);
  client.setWriteOptions(WritePrecision::NoTime, 1, 5);

  TEST_ASSERT(!client.validateConnection());
  for(int i=0;i<12;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    TEST_ASSERT(!client.writePoint(*p));
    delete p;
  }
  TEST_ASSERT(client.isBufferFull());
  TEST_ASSERT(client.getBuffer()[0].indexOf("index=10i")>0);
  
  client.setServerUrl(INFLUXDB_CLIENT_TESTING_URL);
  waitServer(client, true);
  
  Point *p = createPoint("test1");
  p->addField("index", 12);
  TEST_ASSERT(client.writePoint(*p));
  TEST_ASSERT(client.isBufferEmpty());

  String query = "select";
  String q = client.queryString(query);
  int count;
  String *lines = getLines(q,count);
  TEST_ASSERTM(count == 6, String("6 != "+count));    //5 points+header
  TEST_ASSERT(lines[1].indexOf(",8")>0);
  TEST_ASSERT(lines[2].indexOf(",9")>0);
  TEST_ASSERT(lines[3].indexOf(",10")>0);
  TEST_ASSERT(lines[4].indexOf(",11")>0);
  TEST_ASSERT(lines[5].indexOf(",12")>0);
  delete [] lines;
  
  TEST_END();
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);
}

void testBufferOverwriteBatchsize5() {
  TEST_INIT("testBufferOverwriteBatchsize5");
  InfluxDBClient client(INFLUXDB_CLIENT_TESTING_BAD_URL, INFLUXDB_CLIENT_TESTING_ORG,INFLUXDB_CLIENT_TESTING_BUC,INFLUXDB_CLIENT_TESTING_TOK);
  client.setWriteOptions(WritePrecision::NoTime, 5, 12);

  TEST_ASSERT(!client.validateConnection());
  for(int i=0;i<27;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    //will succeed only first batchsize-1 points
    TEST_ASSERTM(client.writePoint(*p)==(i<4), String("i=")+i);
    delete p;
  }
  TEST_ASSERT(client.isBufferFull());
  TEST_ASSERT(client.getBuffer()[0].indexOf("index=24i")>0);
  
  client.setServerUrl(INFLUXDB_CLIENT_TESTING_URL);
  waitServer(client, true);
  
  Point *p = createPoint("test1");
  p->addField("index", 27);
  TEST_ASSERT(client.writePoint(*p));
  TEST_ASSERT(client.isBufferEmpty());

  String query = "select";
  String q = client.queryString(query);
  int count;
  String *lines = getLines(q, count);
  TEST_ASSERT(count == 13); //12 points+header
  TEST_ASSERT(lines[1].indexOf(",16")>0);
  TEST_ASSERT(lines[2].indexOf(",17")>0);
  TEST_ASSERT(lines[3].indexOf(",18")>0);
  TEST_ASSERT(lines[4].indexOf(",19")>0);
  TEST_ASSERT(lines[5].indexOf(",20")>0);
  TEST_ASSERT(lines[12].indexOf(",27")>0);
  delete []lines;
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);
  // buffer has been emptied, now writes should go according batch size
  for(int i=0;i<4;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    TEST_ASSERT(client.writePoint(*p));
    delete p;
  }
  TEST_ASSERT(!client.isBufferEmpty());
  q = client.queryString(query);
  TEST_ASSERT(countLines(q) ==0);

  p = createPoint("test1");
  p->addField("index", 4);
  TEST_ASSERT(client.writePoint(*p));
  TEST_ASSERT(client.isBufferEmpty());
  q = client.queryString(query);
  lines = getLines(q, count);
  TEST_ASSERT(count == 6);//5 points+header
  TEST_ASSERT(lines[1].indexOf(",0")>0);
  TEST_ASSERT(lines[2].indexOf(",1")>0);
  TEST_ASSERT(lines[3].indexOf(",2")>0);
  TEST_ASSERT(lines[4].indexOf(",3")>0);
  TEST_ASSERT(lines[5].indexOf(",4")>0);
  delete [] lines;
 
  TEST_END();
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);
}

void testServerTempDownBatchsize5() {
  TEST_INIT("testServerTempDownBatchsize5");
  InfluxDBClient client(INFLUXDB_CLIENT_TESTING_URL, INFLUXDB_CLIENT_TESTING_ORG,INFLUXDB_CLIENT_TESTING_BUC,INFLUXDB_CLIENT_TESTING_TOK);
  client.setWriteOptions(WritePrecision::NoTime, 5, 20, 60, true);
  
  TEST_ASSERT(client.validateConnection());
  for(int i=0;i<15;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    TEST_ASSERTM(client.writePoint(*p), String("i=")+i);
    delete p;
  }
  TEST_ASSERT(client.isBufferEmpty());
  String query = "select";
  String q = client.queryString(query);
  TEST_ASSERT(countLines(q) == 16); //15 points+header
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);
  
  Serial.println("Stop server");
  waitServer(client, false);
  for(int i=0;i<15;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    //will succeed only first batchsize-1 points
    TEST_ASSERTM(client.writePoint(*p)==(i<4), String("i=")+i);
    delete p;
  }
  TEST_ASSERT(!client.isBufferEmpty());

  Serial.println("Start server");;
  waitServer(client, true);
  
  Point *p = createPoint("test1");
  p->addField("index", 15);
  TEST_ASSERT(client.writePoint(*p));
  TEST_ASSERT(client.isBufferEmpty());
  q = client.queryString(query);
  TEST_ASSERT(countLines(q) == 17); //16 points+header
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);

  Serial.println("Stop server");
  waitServer(client, false);

  for(int i=0;i<25;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    //will succeed only first batchsize-1 points
    TEST_ASSERTM(client.writePoint(*p)==(i<4), String("i=")+i);
    delete p;
  }
  TEST_ASSERT(client.isBufferFull());
  
  Serial.println("Start server");;
  waitServer(client, true);

  TEST_ASSERT(client.flushBuffer());
  q = client.queryString(query);
  int count;
  String *lines = getLines(q,count);
  TEST_ASSERT(count == 21); //20 points+header
  TEST_ASSERT(lines[1].indexOf(",5")>0);
  TEST_ASSERT(lines[2].indexOf(",6")>0);
  TEST_ASSERT(lines[3].indexOf(",7")>0);
  TEST_ASSERT(lines[4].indexOf(",8")>0);
  TEST_ASSERT(lines[19].indexOf(",23")>0);
  TEST_ASSERT(lines[20].indexOf(",24")>0);
  delete [] lines;
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);
 
  
  TEST_END();
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);
}

void testRetriesOnServerOverload() {
  TEST_INIT("testRetriesOnServerOverload");
  InfluxDBClient client(INFLUXDB_CLIENT_TESTING_URL, INFLUXDB_CLIENT_TESTING_ORG,INFLUXDB_CLIENT_TESTING_BUC,INFLUXDB_CLIENT_TESTING_TOK);
  client.setWriteOptions(WritePrecision::NoTime, 5, 20, 60, false);
  
  TEST_ASSERT(client.validateConnection());
  for(int i=0;i<60;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    TEST_ASSERTM(client.writePoint(*p), String("i=")+i);
    delete p;
  }
  TEST_ASSERT(client.isBufferEmpty());
  String query = "select";
  String q = client.queryString(query);
  TEST_ASSERT(countLines(q) == 61); //60 points+header
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);

  String rec = "a,direction=429-1 a=1";
  TEST_ASSERT(client.writeRecord(rec));
  TEST_ASSERT(!client.flushBuffer());
  client.resetBuffer();

  uint32_t start = millis();
  uint32_t retryDelay = 30;
  for(int i=0;i<50;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    uint32_t dur = (millis()-start)/1000; 
    if(client.writePoint(*p)) {
      if(i>=4) {
        TEST_ASSERTM(dur >= retryDelay, String("Too early write: ")+dur);
      } 
    } else {
      TEST_ASSERTM(i>=4, String("i=")+i);
      if(dur >= retryDelay) {
        TEST_ASSERTM(false, String("Write should be ok: ")+ dur);
      } 
    }
    delete p;
    delay(1000);
  }
  TEST_ASSERT(!client.isBufferEmpty());
  TEST_ASSERT(client.flushBuffer());
  TEST_ASSERT(client.isBufferEmpty());
  q = client.queryString(query);
  int count;
  String *lines = getLines(q,count);
  TEST_ASSERT(count == 40); //39 points+header
  TEST_ASSERT(lines[1].indexOf(",11")>0);
  TEST_ASSERT(lines[39].indexOf(",49")>0);
  delete [] lines;
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);

  rec = "a,direction=429-2 a=1";
  TEST_ASSERT(client.writeRecord(rec));
  TEST_ASSERT(!client.flushBuffer());
  client.resetBuffer();

  retryDelay = 60;
  start = millis();
  for(int i=0;i<50;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    uint32_t dur = (millis()-start)/1000; 
    if(client.writePoint(*p)) {
      if(i>=4) {
        TEST_ASSERTM(dur >= retryDelay, String("Too early write: ")+dur);
      } 
    } else {
      TEST_ASSERTM(i>=4, String("i=")+i);
      if(dur >= retryDelay) {
        TEST_ASSERTM(false, String("Write should be ok: ")+ dur);
      } 
    }
    delete p;
    delay(2000);
  }
  TEST_ASSERT(!client.isBufferEmpty());
  TEST_ASSERT(client.flushBuffer());
  TEST_ASSERT(client.isBufferEmpty());
  q = client.queryString(query);
  lines = getLines(q,count);
  TEST_ASSERT(count == 40); //39 points+header
  TEST_ASSERT(lines[1].indexOf(",11")>0);
  TEST_ASSERT(lines[39].indexOf(",49")>0);
  delete [] lines;
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);

  rec = "a,direction=503-1 a=1";
  TEST_ASSERT(client.writeRecord(rec));
  TEST_ASSERT(!client.flushBuffer());
  client.resetBuffer();

  retryDelay = 10;
  start = millis();
  for(int i=0;i<50;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    uint32_t dur = (millis()-start)/1000; 
    if(client.writePoint(*p)) {
      if(i>=4) {
        TEST_ASSERTM(dur >= retryDelay, String("Too early write: ")+dur);
      } 
    } else {
      TEST_ASSERTM(i>=4, String("i=")+i);
      if(dur >= retryDelay) {
        TEST_ASSERTM(false, String("Write should be ok: ")+ dur);
      } 
    }
    delete p;
    delay(1000);
  }
  TEST_ASSERT(!client.isBufferEmpty());
  TEST_ASSERT(client.flushBuffer());
  TEST_ASSERT(client.isBufferEmpty());
  q = client.queryString(query);
  TEST_ASSERT(countLines(q) == 51); //50 points+header
  lines = getLines(q,count);
  TEST_ASSERT(count == 51); //50 points+header
  TEST_ASSERT(lines[1].indexOf(",0")>0);
  TEST_ASSERT(lines[50].indexOf(",49")>0);
  delete [] lines;
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);

  rec = "a,direction=503-2 a=1";
  TEST_ASSERT(client.writeRecord(rec));
  TEST_ASSERT(!client.flushBuffer());
  client.resetBuffer();

  retryDelay = 60;
  start = millis();
  for(int i=0;i<50;i++) {
    Point *p = createPoint("test1");
    p->addField("index", i);
    uint32_t dur = (millis()-start)/1000; 
    if(client.writePoint(*p)) {
      if(i>=4) {
        TEST_ASSERTM(dur >= retryDelay, String("Too early write: ")+dur);
      } 
    } else {
      TEST_ASSERTM(i>=4, String("i=")+i);
      if(dur >= retryDelay) {
        TEST_ASSERTM(false, String("Write should be ok: ")+ dur);
      } 
    }
    delete p;
    delay(2000);
  }
  TEST_ASSERT(!client.isBufferEmpty());
  TEST_ASSERT(client.flushBuffer());
  TEST_ASSERT(client.isBufferEmpty());
  
  q = client.queryString(query);
  lines = getLines(q,count);
  TEST_ASSERT(count == 40); //39 points+header
  TEST_ASSERT(lines[1].indexOf(",11")>0);
  TEST_ASSERT(lines[39].indexOf(",49")>0);
  delete [] lines;

  TEST_END();
  deleteAll(INFLUXDB_CLIENT_TESTING_URL);
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