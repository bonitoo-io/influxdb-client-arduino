#ifndef _TEST_SUUPORT_H_
#define _TEST_SUUPORT_H_

#define TEST_INIT(name) int temp = failures; do { Serial.println(name)
#define TEST_END(name)  } while(0); Serial.printf("%s %s\n",name,failures == temp?"SUCCEEDED":"FAILED");
#define TEST_ASSERT(a) if(testAssert(__LINE__, (a))) break;

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

#endif //_TEST_SUUPORT_H_