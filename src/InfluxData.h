/**
    ESP8266 InfluxDb: InfluxData

    Purpose: Holds the data of a single measurement.

    @see
   https://docs.influxdata.com/influxdb/v1.5/concepts/glossary/#measurement

    @author Tobias Schürg
*/
#include "InfluxDbClient.h"

class InfluxData : public Point {
 public:
  InfluxData(String measurement) : Point(measurement) {}

  void addValue(String key, float value) { addField(key, value); }
  void addValueString(String key, String value) { addField(key, value); }
  void setTimestamp(long int seconds) { setTime(seconds); }

  String toString() const { return toLineProtocol(); }
};
