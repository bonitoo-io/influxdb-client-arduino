#ifndef _INFLUXDB_CLIENT_H_
#define _INFLUXDB_CLIENT_H_

#include "Arduino.h"

#if defined(ESP8266)
# include <WiFiClientSecureBearSSL.h>
# include <ESP8266HTTPClient.h>
#elif defined(ESP32)
# include <HTTPClient.h>
#else
# error “This library currently supports only ESP8266 and ESP32.”
#endif

#ifdef USING_AXTLS
#error AxTLS doesn't work
#endif

 // Enum WritePrecision defines constants for specifying InfluxDB write prcecision
 enum class WritePrecision  {
  // Specifyies that points has no timestamp (default) 
  NoTime = 0,
  // Seconds
  S,
  // Milli-seconds 
  MS,
  // Micro-seconds 
  US,
  // Nano-seconds
  NS
};

// Class Point represents InfluxDB point in line protocol.
// It defines data to be written to InfluxDB.
class Point {
  public:
    Point(String measurement);
    // Adds string tag 
    void addTag(String name, String value);
    // Add field with various types
    void addField(String name, float value)         { if(!isnan(value)) putField(name, String(value)); }
    void addField(String name, double value)        { if(!isnan(value)) putField(name, String(value)); }
    void addField(String name, char value)          { putField(name, String(value)); }
    void addField(String name, unsigned char value) { putField(name, String(value)+"i"); }
    void addField(String name, int value)           { putField(name, String(value)+"i"); }
    void addField(String name, unsigned int value)  { putField(name, String(value)+"i"); }
    void addField(String name, long value)          { putField(name, String(value)+"i"); }
    void addField(String name, unsigned long value) { putField(name, String(value)+"i"); }
    void addField(String name, bool value)          { putField(name,value?"true":"false"); }
    void addField(String name, String value)        { addField(name, value.c_str()); }
    void addField(String name, const char *value);
    // Set timestamp to `now()` and store it in specified precision. Date and time must be already set. See `configTime` in API
    void setTime(WritePrecision writePrecision);
    // Set timestamp in desired precision (specified in InfluxDBClient) since epoch (1.1.1970 00:00:00)
    void setTime(unsigned long timestamp);
    // Set timestamp in desired precision (specified in InfluxDBClient) since epoch (1.1.1970 00:00:00)
    void setTime(String timestamp);
    // Clear all fields. Usefull for reusing point  
    void clearFields();
    // Clear tags
    void clearTags();
    // True if a point contains at least one field. Points without a field cannot be written to db
    bool hasFields() const { return _fields.length() > 0; }
    // True if a point contains at least one tag
    bool hasTags() const   { return _tags.length() > 0; }
     // True if a point contains timestamp
    bool hasTime() const   { return _timestamp.length() > 0; }
    // Creates line protocol
    String toLineProtocol();
  private:
    String _tags;
    String _fields;
    String _measurement;
    String _timestamp;    
    // method for formating field into line protocol
    void putField(String name, String value);
};

// InfluxDBClient handles connection and basic operations for InfluxDB 2
// It provides write API with ability to write data in batches, retrying failure writes, smart optimization and simple Flux querying
// Automaticaly retries failed writes during next write, if server is overloaded.
class InfluxDBClient {
  public:
    // Creates InfluxDBClient instance for unsecured connection
    // serverUrl - url of the InfluxDB 2 server (e.g. http://localhost:9999)
    // org - name of the organization, which bucket belongs to    
    // bucket - name of the bucket to write data into
    // authToken - InfluxDB 2 authorization token
    InfluxDBClient(const char *serverUrl, const char *org, const char *bucket, const char *authToken);
    // Creates InfluxDBClient instance for secured connection
    // serverUrl - url of the InfluxDB 2 server (e.g. https://localhost:9999)
    // org - name of the organization, which bucket belongs to 
    // bucket - name of the bucket to write data into
    // authToken - InfluxDB 2 authorization token
    // serverCert - InfluxDB 2 server trusted certificate (or CA certificate) or certificate SHA1 fingerprint. Should be stored in PROGMEM.
    InfluxDBClient(const char *serverUrl, const char *org, const char *bucket, const char *authToken, const char *serverCert);
    // Destructor. Cleans wifiClient
    ~InfluxDBClient();
    // precision - timestamp precision of written data
    // batchSize - number of points that will be written to the databases at once. Default 1 - writes immediately
    // bufferSize - maximum size of Points buffer. Buffer contains new data that will be written to the database
    //             and also data that failed to be written due to network failure or server overloading
    // flushInterval - maximum number of seconds data will be held in buffer before are written to the db. 
    //                 Data are written either when number of points in buffer reaches batchSize or time of  
    // preserveConnection - true if HTTP connection should be kept open. Usable for often writes.
    void setWriteOptions(WritePrecision precision, uint16_t batchSize = 1, uint16_t bufferSize = 5, uint16_t flushInterval = 60, bool preserveConnection = true); 
    // Validates connection parameters by conecting to server
    // Returns true if successful, false in case of any erro
    bool validateConnection();
    // Writes record in InfluxDB line protocol format to buffer
    // Returns true if successful, false in case of any error 
    bool writeRecord(String &record);
    // Writes record represented by Point to buffer
    // Returns true if successful, false in case of any error 
    bool writePoint(Point& point);
    // Sends Flux query and returns raw JSON formatted response
    // Return empty string in case of error
    String queryString(String &fluxQuery);
    // Writes all points in buffer, with respect to the batch size, and in case of success clears the buffer.
    // Returns true if successful, false in case of any error 
    bool flushBuffer();
    // Returns true if points buffer is full. Usefull when server is overloaded and we may want increase period of write points or decrease number of points
    bool isBufferFull() const  { return _bufferCeiling == _bufferSize; };
    // Returns true if buffer is empty. Usefull when going to sleep and check if there is sth in write buffer (it can happens when batch size if bigger than 1). Call flushBuffer() then.
    bool isBufferEmpty() const { return _bufferCeiling == 0; };
    // Checks points buffer status and flushes if number of points reached batch size or flush interval run out
    // Returns true if successful, false in case of any error 
    bool checkBuffer();
    // Wipes out buffered points
    void resetBuffer();
    // Returns HTTP status of last request to server. Usefull for advanced handling of failures.
    int getLastStatusCode() const { return _lastStatusCode;  }
    // Returns last response when operation failed
    String getLastErrorMessage() const { return _lastErrorResponse; }
    String getServerUrl() const { return _serverUrl; }
  protected:
    // Checks params and sets up security, if needed
    void init();
    void postRequest(int expectedStatusCode);
    void preRequest();
  private:
    // Connection info
    String _serverUrl;
    String _bucket;
    String _org;
    String _authToken;
    // Cached full write url
    String _writeUrl;
    // Cached full query url
    String _queryUrl;
    // Points timestamp precision. 
    WritePrecision _writePrecision = WritePrecision::NoTime;
    // Number of points that will be written to the databases at once. 
    // Default 1 (immediate write, no batching)
    uint16_t _batchSize = 1;
    // Points buffer
    String *_pointsBuffer = nullptr;
    // Rewrites buffer size - maximum number of record to keep.
    // When max size is reached, oldest records are overwritten
    uint16_t _bufferSize = 5;
    // maximum number of seconds data will be held in buffer before are written to the db. 
    uint16_t _flushInterval = 60;
    // Index to buffer where to store new line
    uint16_t _bufferPointer = 0;
    // Actual count of lines in buffer 
    uint16_t _bufferCeiling = 0;
    // Index of start for next write
    uint16_t _batchPointer = 0;
    // Last time in sec bufer has been sucessfully flushed
    uint32_t _lastFlushed = 0;
    // Last time in ms we made are a request to server
    uint32_t _lastRequestTime = 0;
    // HTTP status code of last request to server
    int _lastStatusCode = 0;
    // Server reponse or library error message for last failed request
    String _lastErrorResponse;
    // Underlying HTTPClient instance 
    HTTPClient _httpClient;
    // Underlying conenction object 
    WiFiClient *_wifiClient = nullptr;
    // Certificate info
    const char *_certInfo = nullptr;
#ifdef  ESP8266
  BearSSL::X509List *_cert = nullptr;   
#endif
    // Store retry timeout suggested by server after last request
    int _lastRetryAfter;
    // internal helper methods
    int postData(const char *data);
    char *prepareBatch(int &size);
    void setUrls();
#ifdef INFLUXDB_CLIENT_TESTING
public:
    String *getBuffer() { return _pointsBuffer; }
    void setServerUrl(const char *serverUrl) {
      _serverUrl = serverUrl;
      setUrls();
    }
#endif         
};


#endif //_INFLUXDB_CLIENT_H_
