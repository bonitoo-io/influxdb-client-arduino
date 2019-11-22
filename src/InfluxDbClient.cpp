/**
 * 
 * 
 */
#include "InfluxDbClient.h"

//#define INFLUXDB_CLIENT_DEBUG

#ifdef INFLUXDB_CLIENT_DEBUG
# define INFLUXDB_CLIENT_DEBUG(fmt, ...) Serial.printf_P( (PGM_P)PSTR(fmt), ## __VA_ARGS__ )
#else
# define INFLUXDB_CLIENT_DEBUG(fmt, ...)
#endif

static const char RetryAfter[] = "Retry-After";

static String escapeKey(String key);
static String escapeValue(String value);
static String escapeJSONString(String &value);

static String precisionToString(WritePrecision precision) {
    switch(precision) {
        case WritePrecision::US:
            return "us";
        case WritePrecision::MS:
            return "ms";
        case WritePrecision::NS:
            return "ns";
        case WritePrecision::S:
            return "s";
        default:
            return "";
    }
}

Point::Point(String measurement):
    _measurement(measurement),
    _tags(""),
    _fields(""),
    _timestamp("")
{

}

void Point::addTag(String name, String value) {
    if(_tags.length() > 0) {
        _tags += ',';
    }
    _tags += escapeKey(name);
    _tags += '=';
    _tags += escapeValue(value);
}

void Point::addField(String name, float value) {
   putField(name, String(value));
}

void Point::addField(String name, int value) {
    putField(name, String(value)+"i");
}


void Point::addField(String name, String value) {
    putField(name, "\"" + escapeValue(value) + "\"");
}

void Point::addField(String name, bool value) {
     putField(name,value?"true":"false");
}

void Point::putField(String name, String value) {
    if(_fields.length() > 0) {
        _fields += ',';
    }
    _fields += escapeKey(name);
    _fields += '=';
    _fields += value;
}

String Point::toLineProtocol() {
    String line =  _measurement + "," + _tags + " " + _fields;
    if(_timestamp != "") {
        line += " " + _timestamp;
    }
    return line;
}

void  Point::setTime(WritePrecision precision) {
    static char buff[7];
    time_t now = time(nullptr);
    switch(precision) {
        case WritePrecision::US:
             sprintf(buff, "%06d",  micros()%1000000uL);
            _timestamp = String(now) + buff;
            break;
        case WritePrecision::MS:
             sprintf(buff, "%03d",  millis()%1000u);
            _timestamp = String(now) + buff;
            break;
        case WritePrecision::NoTime:
            _timestamp = "";
            break;
        case WritePrecision::S:
             _timestamp = String(now);
             break;
    }
}

void  Point::setTime(unsigned long timestamp) {
    _timestamp = String(timestamp);
}
void  Point::setTime(String timestamp) {
    _timestamp = timestamp;
}

void  Point::clearFields() {
    _fields = "";
    _timestamp = "";
}

void Point:: clearTags() {
    _tags = "";
}

InfluxDBClient::InfluxDBClient(const char *serverUrl, const char *org, const char *bucket, const char *authToken):InfluxDBClient(serverUrl, org, bucket, authToken, nullptr) { 
}

InfluxDBClient::InfluxDBClient(const char *serverUrl, const char *org, const char *bucket, const char *authToken, const char *serverCert) {
    _serverUrl = serverUrl;
    _bucket = bucket;
    _org = org;
    _authToken = authToken;
    init(serverCert);
}

void InfluxDBClient::init(const char *pstrCert) {
    if(_serverUrl.endsWith("/")) {
        _serverUrl = _serverUrl.substring(0,_serverUrl.length()-1);
    }
    setUrls();
    bool https = _serverUrl.startsWith("https");
    if(https) {
#if defined(ESP8266)         
        BearSSL::WiFiClientSecure *wifiClientSec = new BearSSL::WiFiClientSecure;
#elif defined(ESP32)
        WiFiClientSecure *wifiClientSec = new WiFiClientSecure;   
#endif        
        if(pstrCert && strlen_P(pstrCert) > 0) {
#if defined(ESP8266) 
            if(strlen_P(pstrCert) > 60 ) { //differentiate fingerprint and cert
                _cert = new BearSSL::X509List(pstrCert); 
                wifiClientSec->setTrustAnchors(_cert);
            } else {
                wifiClientSec->setFingerprint(pstrCert);
            }
#elif defined(ESP32)   
            wifiClientSec->setCACert(pstrCert);
#endif   
        }
        _wifiClient = wifiClientSec;
    } else {
        _wifiClient = new WiFiClient;
#if defined(ESP8266) 
        _cert = nullptr;
#endif        
    }
    _pointsBuffer = new String[_bufferSize];
}

InfluxDBClient::~InfluxDBClient() {
    if(_wifiClient) {
        delete _wifiClient;
        _wifiClient = nullptr;
    }
#if defined(ESP8266)     
    if(_cert) {
        delete _cert;
        _cert = nullptr;
    }
#endif
}

void InfluxDBClient::setUrls() {
    _writeUrl = _serverUrl + "/api/v2/write?org=" + _org + "&bucket=" + _bucket;
    if(_precision != WritePrecision::NoTime) {
        _writeUrl += String("&precision=") + precisionToString(_precision);
    }
    _queryUrl = _serverUrl + "/api/v2/query?org=" + _org;
}

void InfluxDBClient::setWriteOptions(WritePrecision precision, uint16_t batchSize, uint16_t bufferSize, uint16_t flushInterval, bool preserveConnection) {
    if(_precision != precision) {
        _precision = precision;
        setUrls();
    }
    if(batchSize > 0) {
        _batchSize = batchSize;
    }
    if(_bufferSize > 0 && bufferSize > 0 && _bufferSize != bufferSize) {
        _bufferSize = bufferSize;
        if(_bufferSize <  _batchSize) {
            _bufferSize = 2*_batchSize;
            INFLUXDB_CLIENT_DEBUG("[D] Changing buffer size to %d\n", _bufferSize);
        }
        if(_pointsBuffer) {
            delete [] _pointsBuffer;
        }
        _pointsBuffer = new String[_bufferSize];
        _bufferPointer = 0;
        _batchPointer = 0;
        _bufferCeiling = 0;
    }
    _flushInterval = flushInterval;
    _httpClient.setReuse(preserveConnection);
}

bool InfluxDBClient::validateConnection() {
    String url = _serverUrl + "/ready";
    INFLUXDB_CLIENT_DEBUG("[D] Validating connection to %s\n", url.c_str());

    if(!_httpClient.begin(*_wifiClient, url)) {
        INFLUXDB_CLIENT_DEBUG("[E] begin failed");
        return false;
    }
    _httpClient.addHeader(F("Accept"), F("application/json"));
    
    int statusCode = _httpClient.GET();

#ifdef INFLUXDB_CLIENT_DEBUG     
    INFLUXDB_CLIENT_DEBUG("[D] HTTP status code - %d\n", statusCode);
    if(statusCode > 0) {               
        String result = _httpClient.getString();
        if(result.length() > 0) {
           INFLUXDB_CLIENT_DEBUG("[D] Response:\n%s\n", result.c_str());
        }
    } else {
        String result =  _httpClient.errorToString(statusCode);
        if(result.length() > 0) {
           INFLUXDB_CLIENT_DEBUG("[E] Error - %s\n", result.c_str());
        }
    }
#endif //  INFLUXDB_CLIENT_DEBUG  
    _httpClient.end();

    return statusCode == 200;
}

bool InfluxDBClient::writePoint(Point & point) {
    if (point.hasFields()) {
        String line = point.toLineProtocol();
        return writeRecord(line);
    }
    return false;
}

bool InfluxDBClient::writeRecord(String &record) {
    _pointsBuffer[_bufferPointer] = record;
    _bufferPointer++;
    if(_bufferPointer == _bufferSize) {
        _bufferPointer = 0;
        INFLUXDB_CLIENT_DEBUG("[W] Reached buffer size, old points will be overwritten\n");
    } 
    if(_bufferCeiling < _bufferSize) {
        _bufferCeiling++;
    }
    return checkBuffer();
}

bool InfluxDBClient::checkBuffer() {
    // in case we (over)reach batchSize with non full bufer
    bool bufferReachedBatchsize = !isBufferFull() && _bufferPointer - _batchPointer >= _batchSize;
    // or flush interval timed out
    bool flushTimeout = _flushInterval > 0 && (millis()/1000 - _lastFlushed) > _flushInterval; 

    if(bufferReachedBatchsize || flushTimeout || isBufferFull() ) {
        INFLUXDB_CLIENT_DEBUG("[D] Flushing buffer: is oversized %s, is timeout %s, is buffer full %s\n", bufferReachedBatchsize?"true":"false",flushTimeout?"true":"false", isBufferFull()?"true":"false");
       return flushBuffer();
    } 
    return true;
}

bool InfluxDBClient::flushBuffer() {
    if(_lastRetryAfter > 0 && (millis()-_lastRequestTime)/1000 < _lastRetryAfter) {
        // retry after period didn't run out yet
        return false;
    }
    char *data;
    int size;
    bool success = false;
    // send all batches, It could happen there was long network outage and buffer is full
    while(data = prepareBatch(size)) {
        INFLUXDB_CLIENT_DEBUG("[D] Writing batch, size %d\n", size);
        int statusCode = postData(data);
        delete [] data;
        // retry on unsuccessfull connection or retryable status codes
        bool retry = statusCode < 0 || statusCode == 429 || statusCode == 503;
        success = statusCode == 204;
        // advance even on (not likely) failure 
        if(success || !retry) {
            _lastFlushed = millis()/1000;
            _batchPointer += size;
            //did we got over top?
            if(_batchPointer >= _bufferSize) {
                // restart _batchPointer in ring buffer from start
                _batchPointer = _batchPointer - _bufferSize;
            }
        } else {
            INFLUXDB_CLIENT_DEBUG("[D] Leaving data in buffer for retry");
            // in case of retryable failure break loop
            break;
        }
        //small delay between batches
        delay(1);
    }
    //Have we emptied the buffer?
    if(_batchPointer == _bufferPointer) {
        _bufferPointer = 0;
        _batchPointer = 0;
        _bufferCeiling = 0;
        INFLUXDB_CLIENT_DEBUG("[D] Buffer empty\n");
    }
    return success;
}

bool InfluxDBClient::isBufferFull() {
    return _bufferCeiling == _bufferSize;
}

bool InfluxDBClient::isBufferEmpty() {
    return _bufferPointer == 0;
}

char *InfluxDBClient::prepareBatch(int &size) {
    size = 0;
    int length = 0;
    char *buff = nullptr;
    uint16_t top = _batchPointer+_batchSize;
    INFLUXDB_CLIENT_DEBUG("[D] Prepare batch: bufferPointer: %d, batchPointer: %d, ceiling %d\n", _bufferPointer, _batchPointer, _bufferCeiling);
    if(top > _bufferCeiling ) {
        // are we returning to the begging?
        if(isBufferFull()) {
            top = top - _bufferCeiling;
            // in case we are writing points in the begging of the buffer that have been overwritten, end on _bufferPointer
            if(top > _bufferPointer) {
                top = _bufferPointer;
            }
        } else {
            top = _bufferCeiling;
        }
    } 
    if(top > _batchPointer) { 
        size = top - _batchPointer;
    } else if(top < _batchPointer) {
        size = _bufferSize - (_batchPointer - top);
    }
    INFLUXDB_CLIENT_DEBUG("[D] Prepare batch size %d\n", size);
    if(size) {
        int i = _batchPointer;
        for(int c=0; c < size; c++) {
            length += _pointsBuffer[i++].length();
            if(i == _bufferSize) {
                i = 0;
            }
        }
        //create buffer for all lines including new line char and terminating char
        buff = new char[length + size + 1];
        if(buff) {
            buff[0] = 0;
            int i = _batchPointer;
            for(int c=0; c < size; c++) {
                strcat(buff+strlen(buff), _pointsBuffer[i++].c_str());
                strcat(buff+strlen(buff), "\n");
                if(i == _bufferSize) {
                    i = 0;
                }
            }
        } else {
            size = 0;
        }
    }
    return buff;
}

int InfluxDBClient::postData(const char *data) {
    int statusCode = 0;
    if(data) {
        INFLUXDB_CLIENT_DEBUG("[D] Writing to %s\n", _writeUrl.c_str());
        if(!_httpClient.begin(*_wifiClient, _writeUrl)) {
            INFLUXDB_CLIENT_DEBUG("[E] Begin failed\n");
            return false;
        }
        
        _httpClient.addHeader(F("Accept"), F("application/json"));   
        _httpClient.addHeader(F("Authorization"), "Token " + _authToken);
        
        INFLUXDB_CLIENT_DEBUG("[D] Sending:\n%s\n", data);       
        
        const char * headerKeys[] = {RetryAfter} ;
        _httpClient.collectHeaders(headerKeys, 1);
        
        statusCode = _httpClient.POST((uint8_t*)data, strlen(data));
        
        _lastHTTPcode = statusCode;
        _lastRequestTime = millis();

        if(statusCode == 429 || statusCode == 503) { //retryable 
            if(_httpClient.hasHeader(RetryAfter)) {
                int retry = _httpClient.header(RetryAfter).toInt();
                if(retry > 0 ) {
                    _lastRetryAfter = retry;
                }
            }
        } else {
            _lastRetryAfter = 0;
        }
    #ifdef INFLUXDB_CLIENT_DEBUG      
        INFLUXDB_CLIENT_DEBUG("[D] HTTP status code - %d\n", statusCode);
        if(statusCode > 0) {
            if(statusCode != 204) { // skip no data responses, HTTPClient::getString gets stuck on ESP32 in such case
                String result = _httpClient.getString();
                INFLUXDB_CLIENT_DEBUG("[D] Response:\n%s\n", result.c_str());
            }
        } else {
            String result =  _httpClient.errorToString(statusCode);
            if(result.length() > 0) {
                INFLUXDB_CLIENT_DEBUG("[E] Error - %s\n", result.c_str());
            }       
        }
    #endif //  INFLUXDB_CLIENT_DEBUG  
        _httpClient.end();
    } 
    return statusCode;
}

static const char QueryDialect[] PROGMEM = "\
\"dialect\": {\
\"annotations\": [\
\"datatype\",\
\"group\",\
\"default\"\
],\
\"dateTimeFormat\": \"RFC3339\",\
\"header\": true,\
\"delimiter\": \",\",\
\"commentPrefix\": \"#\"\
}}";

String InfluxDBClient::queryString(String &fluxQuery) {
    if(_lastRetryAfter > 0 && (millis()-_lastRequestTime)/1000 < _lastRetryAfter) {
        // retry after period didn't run out yet
        return "";
    }
    INFLUXDB_CLIENT_DEBUG("[D] Query to %s\n", _queryUrl.c_str());

    if(!_httpClient.begin(*_wifiClient, _queryUrl)) {
        INFLUXDB_CLIENT_DEBUG("[E] begin failed\n");
        return "";
    }
    _httpClient.addHeader(F("Content-Type"), F("application/json"));
    _httpClient.addHeader(F("Authorization"), "Token " + _authToken);

    const char * headerKeys[] = {RetryAfter} ;
    _httpClient.collectHeaders(headerKeys, 1);

    String body = "{\"type\":\"flux\",";
    body += "\"query\":\"" + escapeJSONString(fluxQuery) + "\",";
    body += QueryDialect;

    INFLUXDB_CLIENT_DEBUG("[D] JSON query:\n%s\n", body.c_str());

    int statusCode = _httpClient.POST(body);
    _lastHTTPcode = statusCode;
    _lastRequestTime = millis();

    if(statusCode == 429 || statusCode == 503) { //retryable 
        if(_httpClient.hasHeader(RetryAfter)) {
            int retry = _httpClient.header(RetryAfter).toInt();
            if(retry > 0 ) {
                _lastRetryAfter = retry;
            }
        }
    } else {
        _lastRetryAfter = 0;
    }
    INFLUXDB_CLIENT_DEBUG("[D] HTTP status code - %d\n", statusCode);
    String response;
    if(statusCode == 200) {               
        response = _httpClient.getString();
        INFLUXDB_CLIENT_DEBUG("[D] Response:\n%s\n", response.c_str());
    } 
#ifdef INFLUXDB_CLIENT_DEBUG
    if(statusCode != 200) {
        if(statusCode > 0) {
            String result = _httpClient.getString();
            if(result.length() > 0) {
                INFLUXDB_CLIENT_DEBUG("[D] Response:\n%s\n", result.c_str());
            }
        } else {
            String result =  _httpClient.errorToString(statusCode);
            if(result.length() > 0) {
                INFLUXDB_CLIENT_DEBUG("[E] Error - %s\n", result.c_str());
            }
        }
    }
#endif //  INFLUXDB_CLIENT_DEBUG  
    _httpClient.end();

    return response;
}

static String escapeKey(String key) {
    String ret;
    ret.reserve(key.length()+5); //5 is estimate of  chars needs to escape,
    
//    for (char c: key)
    for(int i=0;i<key.length();i++) 
    {
        char c = key[i];
        switch (c)
        {
            case ' ':
            case ',':
            case '=':
                ret += '\\';
                break;
        }

        ret += c;
    }
    return ret;
}

static String escapeValue(String value) {
    String ret;
    ret.reserve(value.length()+5); //5 is estimate of max chars needs to escape,
    //for (char c: value)
    for(int i=0;i<value.length();i++) 
    {
        char c = value[i];
        switch (c)
        {
            case '\\':
            case '\"':
                ret += '\\';
                break;
        }

        ret += c;
    }
    return ret;
}

static String escapeJSONString(String &value) {
    String ret;
    int d = 0;
    int i,from = 0;
    while((i = value.indexOf('"',from)) > -1) {
        d++;
        if(i == value.length()-1) {
            break;
        }
        from = i+1;
    }
    ret.reserve(value.length()+d); //most probably we will escape just double quotes
    //for (char c: value)
    for(int i=0;i<value.length();i++) 
    {
        char c = value[i];
        switch (c)
        {
            case '"': ret += "\\\""; break;
            case '\\': ret += "\\\\"; break;
            case '\b': ret += "\\b"; break;
            case '\f': ret += "\\f"; break;
            case '\n': ret += "\\n"; break;
            case '\r': ret += "\\r"; break;
            case '\t': ret += "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    ret += "\\u";
                    char buf[3 + 8 * sizeof(unsigned int)];
                    sprintf(buf,  "\\u%04u", c);
                    ret += buf;
                } else {
                    ret += c;
                }
        }
    }
    return ret;
}
