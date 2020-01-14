/**
    ESP8266 InfluxDb: Influxdb.cpp

    Purpose: Helps with sending measurements to an Influx database.

    @author Tobias Schürg
*/
#include "InfluxDb.h"
#include "Arduino.h"

/**
 * Construct an InfluxDb instance.
 * @param host the InfluxDb host
 * @param port the InfluxDb port
 */
Influxdb::Influxdb(String host, uint16_t port) {
  if(port == 443) {
    _serverUrl = "https://";
  } else {
    _serverUrl = "http://";
  }
  _serverUrl += host + ":" + String(port);
}

/**
 * Set the database to be used.
 * @param db the Influx Database to be written to.
 */
void Influxdb::setDb(String db) {
  _bucket = db;
}

/**
 * Set the database to be used with authentication.
 */
void Influxdb::setDbAuth(String db, String user, String pass) {
  _bucket = db;
  _user = user;
  _password = pass;
}

/**
 * Set the Bucket to be used v2.0 ONLY.
 * @param bucket the InfluxDB Bucket which must already exist
 */
void Influxdb::setBucket(String bucket) {
  _bucket = bucket;
}

/**
 * Set the influxDB port.
 * @param port v1.x uses 8086, v2 uses 9999
 */
void Influxdb::setPort(uint16_t port){
  int b = _serverUrl.indexOf(":",5);
  if(b > 0) {
    _serverUrl = _serverUrl.substring(0, b+1) + String(port);
  }
}
/**
 * Set the Organization to be used v2.0 ONLY
 * @param org the Name of the organization unit to use which must already exist
 */
void Influxdb::setOrg(String org){
  _org = org;
}

/**
 * Set the authorization token v2.0 ONLY
 * @param token the Auth Token from InfluxDBv2 *required*
 */
void Influxdb::setToken(String token){
  _authToken = token;
}

/**
 * Set the version of InfluxDB to write to
 * @param version accepts 1 for version 1.x or 2 for version 2.x
 */
void Influxdb::setVersion(uint16_t version){
  _dbVersion = version;
}

#if defined(ESP8266)
/**
 * Set server certificate finger print 
 * @param fingerPrint server certificate finger print 
 */
void Influxdb::setFingerPrint(const char *fingerPrint){
  _certInfo = fingerPrint;
}
#endif

void Influxdb::begin() {
 
}

/**
 * Prepare a measurement to be sent.
 */
void Influxdb::prepare(InfluxData data) { 
  ++_preparedPoints;
  if(_batchSize < _preparedPoints) {
    _batchSize = _preparedPoints;
    reserveBuffer(2*_batchSize);
  }
  write(data);
}

/**
 * Write all prepared measurements into the db.
 */
boolean Influxdb::write() {
  return flushBuffer();
}

/**
 * Write a single measurement into the db.
 */
boolean Influxdb::write(InfluxData data) { 
    return write(data.toLineProtocol());
}

/**
 * Send raw data to InfluxDb.
 *
 * @see
 * https://github.com/esp8266/Arduino/blob/cc0bfa04d401810ed3f5d7d01be6e88b9011997f/libraries/ESP8266HTTPClient/src/ESP8266HTTPClient.h#L44-L55
 * for a list of error codes.
 */
boolean Influxdb::write(String data) {
  return writeRecord(data);
}
