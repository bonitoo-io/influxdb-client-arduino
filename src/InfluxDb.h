
/**
    ESP8266 InfluxDb: Influxdb.h

    Purpose: Helps with sending measurements to an Influx database.

    @author Tobias Schürg
*/
#include "InfluxData.h"

class Influxdb : public InfluxDBClient {
 public:
  Influxdb(String host, uint16_t port = 8086);

  void setDb(String db);
  void setDbAuth(String db, String user, String pass);

  void setVersion(uint16_t version);
  void setBucket(String bucket);
  void setOrg(String org);
  void setToken(String token);
  void setPort(uint16_t port);
#if defined(ESP8266)
  void setFingerPrint(const char *fingerPrint);
#endif

  void prepare(InfluxData data);
  boolean write();

  boolean write(InfluxData data);
  boolean write(String data);

 private:
  uint16_t _preparedPoints;
  
  void begin();
};
