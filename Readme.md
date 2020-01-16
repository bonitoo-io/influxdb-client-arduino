# InfluxDB Arduino Client

Simple Arduino client for writing and reading data from [InfluxDB](https://www.influxdata.com/products/influxdb-overview/), it doesn't matter whether a local server or InfluxDB Cloud. Library supports authentication, secure communication over TLS, [batching](#writing_in_batches), [automatic retrying](#write_retrying) on server backpressure and connection failure.

Of course it allows setting data in various formats, automatically escapes special characters and also offers specifying timestamp in various precisions. 

Library support both [InfluxDB 2](#basic_code_for_influxdb_2) and [InfluxDB 1](#basic_code_for_influxdb_2).

This is a new implementation and API, [original API](#original_api) is still supported. 

## Basic code for InfluxDB 2
Using client is very easy. After you've [setup InfluxDB 2 server](https://v2.docs.influxdata.com/v2.0/get-started), first define connection parameters and a client instance:
```cpp
// InfluxDB v2 server url, e.g. http://192.168.1.48:9999 (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "influxdb-url"
// InfluxDB v2 server or cloud API authentication token (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
#define INFLUXDB_TOKEN "token"
// InfluxDB v2 organization id (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
#define INFLUXDB_ORG "org"
// InfluxDB v2 bucket name (Use: InfluxDB UI -> Load Data -> Buckets)
#define INFLUXDB_BUCKET "bucket"

// Single InfluxDB instance
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
```

The next step is adding data. Single data row is represented by the `Point` class. It consists of measurement name (like a table name), tags (which labels data) and fields (values to store):
```cpp
// Define data point with measurement name 'device_status`
Point pointDevice("device_status");
// Set tags
pointDevice.addTag("device", "ESP8266");
pointDevice.addTag("SSID", WiFi.SSID());
// Add data
pointDevice.addField("rssi", WiFi.RSSI());
pointDevice.addField("uptime", millis());
```

And finally we will write data to db:
```cpp
// Write data
client.writePoint(pointDevice);
```

You will able to see data in the InfluxDB UI now. You can use  `Data Explorer` or create a `Dashboard`.

Complete source code is available in [BasicWrite example](examples/BasicWrite/BasicWrite.ino).

## Basic code for InfluxDB 1
Use InfluxDB Arduino client for InfluxDB 1 is almost the same. The only difference is that InfluxDB 1 uses _database_ as classic name for data storage instead of bucket and the server is unsecured by default.
There is just different `InfluxDBClient contructor`. Everything else remains the same.

```cpp
// InfluxDB server url, e.g. http://192.168.1.48:8086 (don't use localhost, always server name or ip address)
#define INFLUXDB_URL "influxdb-url"
// InfluxDB database name 
#define INFLUXDB_DB_NAME "database"

// Single InfluxDB instance
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_DB_NAME);

// Define data point with measurement name 'device_status`
Point pointDevice("device_status");
// Set tags
pointDevice.addTag("device", "ESP8266");
pointDevice.addTag("SSID", WiFi.SSID());
// Add data
pointDevice.addField("rssi", WiFi.RSSI());
pointDevice.addField("uptime", millis());

// Write data
client.writePoint(pointDevice);
```

## Connecting to InfluxDB Cloud 2
Instead of setting up local InfluxDB 2 server, you can quickly [start with InfluxDB Cloud 2](https://v2.docs.influxdata.com/v2.0/cloud/get-started/) with [Free Plan](https://v2.docs.influxdata.com/v2.0/cloud/pricing-plans/#free-plan).

Connecting Arduino client to InfuxDB Cloud server requires few additional steps.
InfluxDBCloud uses secure communication (https) and we need to tell the client to trust this connection.
Connection parameters are almost the same as above, the only difference is that server URL now points to the InfluxDB Cloud 2, where you've got after you've finished creating InfluxDB Cloud 2 subscription. You will find correct server URL in  `InfluxDB UI -> Load Data -> Client Libraries`.
```cpp
// InfluxDB v2 server or cloud url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "influxdb-url"
// InfluxDB v2 server or cloud API authentication token (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
#define INFLUXDB_TOKEN "token"
// InfluxDB v2 organization id (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
#define INFLUXDB_ORG "org"
// InfluxDB v2 bucket name (Use: InfluxDB UI -> Load Data -> Buckets)
#define INFLUXDB_BUCKET "bucket"
```

You need to pass an additional parameter to the client constructor, which is a certificate of the server to trust. Constant `InfluxDbCloud2CACert` contains the InfluxDB Cloud 2 CA certificate, which is predefined in this library: 
```cpp
// Single InfluxDB instance
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
```

Additionally, time needs to be synced:
```cpp
// Synchronize UTC time with NTP servers
// Accurate time is necessary for certificate validaton and writing in batches
configTime(0, 0, "pool.ntp.org", "time.nis.gov");
// Set timezone
setenv("TZ", TZ_INFO, 1);
```
Read more about time synchronization in [Configure Time](#configure_time).

Defining data and writing it to the DB is the same as in the case of [BasicWrite](#basic-code):
```cpp
// Define data point with measurement name 'device_status`
Point pointDevice("device_status");
// Set tags
pointDevice.addTag("device", "ESP8266");
pointDevice.addTag("SSID", WiFi.SSID());
// Add data
pointDevice.addField("rssi", WiFi.RSSI());
pointDevice.addField("uptime", millis());

// Write data
client.writePoint(pointDevice);
```
Complete source code is available in [SecureWrite example](examples/SecureWrite/SecureWrite.ino).

## Writing in Batches
InfluxDB client for Arduino can write data in batches. A batch is simply a set of points that will be sent at once. To create a batch, the client will keep all points until the number of points reaches the batch size and then it will write all points at once to the InfluDB server. This is often more efficient than writing each point separately. 

### Timestamp
If using batch writes, the timestamp should be employed. Timestamp specifies the time where data was gathered and it is used in the form of a number of seconds (milliseconds, etc) from epoch (1.1.1970) UTC.
If points have no timestamp assigned, InfluxDB assigns timestamp at the time of writing, which could happen much later than the data has been obtained, because final batch write will happen when the batch is full (or when [flush buffer](#buffer_handling) is forced).

InfuxDB allows sending timestamp in various precisions - nanoseconds, microseconds, milliseconds or seconds. The milliseconds precision is usually enough for using on Arduino.

The client has to be configured with time precision. The default settings is not using the timestamp. The `setWriteOptions` methods allow setting various parameters and one of them is __write precision__:
``` cpp
// Set write precision to milliseconds. Leave other parameters default.
client.setWriteOptions(WritePrecision::MS);
```
When a write precision is configured, the client will automatically assign current time to the timestamp of each written point, which doesn't have a timestamp assigned. 

If you want to manage timestamp on your own, there are several ways how to set timestamp explicitly.
- `setTime(WritePrecision writePrecision)` - Sets timestamp to actual time in desired precision
- `setTime(unsigned long seconds)` -  Sets timestamp in seconds since epoch. Write precision must be set to `S` 
- `setTime(String timestamp)` - Set custom timestamp in precision specified in InfluxDBClient. 


### Configure Time
Dealing with timestamps requires the device has correctly set time. This can be done with just a few lines of code:
```cpp
// Synchronize UTC time with NTP servers
// Accurate time is necessary for certificate validaton and writing in batches
configTime(0, 0, "pool.ntp.org", "time.nis.gov");
// Set timezone
setenv("TZ", "PST8PDT", 1);
```
The `configTime` method starts the time synchronization with NTP servers. The first two parameters specify DST and timezone offset, but we keep them zero and configure timezone info later.
The last two string parameters are the internet addresses of NTP servers. Check [pool.ntp.org](https://www.pool.ntp.org/zone) for address of some local NTP servers.

Using the `setenv` method with `TZ` param ensures a device has the correct timezone. This is critical for distinguishing UTC and a local timezone because timestamps of points must be set in the UTC timezone.
The second parameter is timezone information, which is described at [https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html). 

Values for some timezones:
- Central Europe: `CET-1CEST,M3.5.0,M10.5.0/3`
- Eastern: `EST5EDT`
- Japanesse: `JST-9`
- Pacific Time: `PST8PDT`

We could set the timezone info (DST and UTC offset) also in the first two parameters of the `configTime` method, but there is a [bug on ESP8266](TODO: link) which causes a time behaves as it is in UTC, even UTC offset was specified.

### Batch Size
Setting batch size depends on data gathering and DB updating strategy.

If data is written in short periods (seconds), batch size should be according to expected write periods and update frequency requirements. 
For example, if you would like to see updates (on the dashboard or in processing) each minute and you are measuring single data (1 point) each 10s (6 points per minute), batch size should be 6. In case it is enough to update each hour and you are creating 1 point at once each minute, your batch size should be 60. The maximum recommended batch size is 200. It depends on the RAM of the device (80KB for ESP8266 and 512KB for ESP32).

In case that data should be written in longer periods and gathered data consists of several points batch size should be set to an expected number of points.

To set batch size we use [setWriteOptions](#write_options) method, where second parameter controls batch size:
```cpp
// Enable messages batching
client.setWriteOptions(WritePrecision::MS, 10);
```
Writing point will add a point to the underlying buffer until the batch size is reached:
```cpp
// Write first point to the buffer
client.writePoint(point1);
// Write second point to the buffer
client.writePoint(point2);
..
// Write nineth point to the buffer, returns 
client.writePoint(point9);
// Writing tenth point will cause flushing buffer
if(!client.writePoint(point10)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
}
```

In case of a number of points is not always the same, set batch size to the maximum number of points and use the `flushBuffer()` method to force writing to DB. See [Buffer Handling](#buffer_handling_and_retrying) for more details.

## Buffer Handling and Retrying
InfluxDB contains an underlying buffer for handling writing in batches and automatic retrying on server backpressure and connection failure.

Its size is controled by the 3rd parameter of [setWriteOptions](#write_options) method:
```cpp
// Enable messages batching
client.setWriteOptions(WritePrecision::MS, 10, 30);
```
The third parameter specifies the buffer size. The recommended size is at least 2 x batch size. 

State of the buffer can be determined via two methods:
 - `isBufferEmpty()` - Returns true if buffer is empty
 - `isBufferFull()` - Returns true if buffer is full
 
 Full buffer can occur when there is a problem with an internet connection or the InfluxDB server is overloaded. In such cases, points to write remains in buffer. When more points are added and connection problem remains, the buffer will reach the top and new points will overwrite older points.

 Each attempt to write a point will try to send older points in the buffer. So, the `isBufferFull()` method can be used to skip low priority points.

The `flushBuffer()` method can be used to force writing, even the number of points in the buffer is lower than the batch size. With the help of the `isBufferEmpty()` method a check can be made before a device goes to sleep:

 ```cpp
  // Check whether buffer in not empty
  if (!client.isBufferEmpty()) {
      // Write all remaining points to db
      client.flushBuffer();
  }
```

Other methods for dealing with buffer:
 - `checkBuffer()` - Checks point buffer status and flushes if the number of points reaches batch size or flush interval runs out. This main method for controlling buffer and it is used internally.
 - `resetBuffer()` - Clears the buffer.

Check [SecureBatchWrite example](examples/SecureBatchWrite/SecureBatchWrite.ino) for example code of buffer handling functions.

## Write Options
Writing points can be controlled via several parameters in `setWriteOptions` method:

| Parameter | Default Value | Meaning |
|-----------|---------------|---------| 
| precision | `WritePrecision::NoTime` | Timestamp precision of written data |
| batchSize | `1` | Number of points that will be written to the database at once |
| bufferSize | `5` | Maximum number of points in buffer. Buffer contains new data that will be written to the database and also data that failed to be written due to network failure or server overloading |
| flushInterval | `60` | Maximum time(in seconds) data will be held in buffer before are written to the db |
| preserveConnection | `false` | true if underlying HTTP connection should be kept open |

## Querying
InfluxDB uses [Flux](https://www.influxdata.com/products/flux/) to process and query data. InfluxDB client for Arduino offers a simple way how to query data with `query` function:
```cpp
// Construct a Flux query
// Query will find RSSI for last 24 hours for each connected WiFi network with this device computed by given selector function
String query = "from(bucket: \"" INFLUXDB_BUCKET "\") |> range(start: -24h) |> filter(fn: (r) => r._measurement == \"wifi_status\" and r._field == \"rssi\"";
query += "and r.device == \"" DEVICE "\")";
query += "|> " + selectorFunction + "()";

String resultSet = client.query(query);
// Check empty response
if (resultSet == "") {
    // It can mean empty query result
    if (client.wasLastQuerySuccessful()) {
        Serial.println("Empty results set");
    } else {
        // or an error
        Serial.print("InfluxDB query failed: ");
        Serial.println(client.getLastErrorMessage());
    }
} else {
    Serial.println(resultSet);
}
``` 

InfluxDB query result set is returned in the CSV format, where the first line contains column names:
```CSV
,result,table,_start,_stop,_time,_value,SSID,_field,_measurement,device
,_result,0,2019-12-11T12:39:49.632459453Z,2019-12-12T12:39:49.632459453Z,2019-12-12T12:26:25Z,-68,666G,rssi,wifi_status,ESP8266
```
This library also provides a couple of helper methods for parsing such a result set.

If the query results in an empty result set, the server returns an empty response. As the empty result returned from the `query` function indicates an error,
use `wasLastQuerySuccessful()` method to determine final status.

Complete source code is available in [Query example](examples/Query/Query.ino).

## Troubleshooting
All db methods return status. `false` means something went wrong. Call `getLastErrorMessage()` to get the error message.

When error message doesn't help to explain the bad behavior, go to the library sources and in the file `src/InfluxDBClient.cpp` uncomment line 30:
```cpp
// Uncomment bellow in case of a problem and rebuild sketch
#define INFLUXDB_CLIENT_DEBUG
```
Then upload your sketch again and see the debug output in the Serial Monitor.

If you couldn't solve a problem by yourself, please, post an issue including the debug output

# Original API


## Initialization
```cpp
 #define INFLUXDB_HOST "192.168.0.32"
 #define INFLUXDB_PORT "1337"
 #define INFLUXDB_DATABASE "test"
 //if used with authentication
 #define INFLUXDB_USER "user"
 #define INFLUXDB_PASS "password"

 // connect to WiFi

 Influxdb influx(INFLUXDB_HOST); // port defaults to 8086, use 9999 for v2
 // or to use a custom port
 Influxdb influx(INFLUXDB_HOST, INFLUXDB_PORT);

 // set the target database
 influx.setDb(INFLUXDB_DATABASE);
 // or use a db with auth
 influx.setDbAuth(INFLUXDB_DATABASE, INFLUXDB_USER, INFLUXDB_PASS) // with authentication

// To use the v2.0 InfluxDB
influx.setVersion(2);
influx.setOrg("myOrganization");
influx.setBucket("myBucket");
influx.setToken("myToken");
influx.setPort(9999);
```

## Sending a single measurement
**Using an InfluxData object:**
```cpp
// create a measurement object
InfluxData measurement ("temperature");
measurement.addTag("device", d2);
measurement.addTag("sensor", "dht11");
measurement.addValue("value", 24.0);

// write it into db
influx.write(measurement);
```

**Using raw-data**
```cpp
 influx.write("temperature,device=d2,sensor=dht11 value=24.0")
```

## Write multiple data points at once
Batching measurements and send them with a single request will result in a much higher performance.
```cpp

InfluxData measurement1 = readTemperature()
influx.prepare(measurement1)

InfluxData measurement2 = readLight()
influx.prepare(measurement2)

InfluxData measurement3 = readVoltage()
influx.prepare(measurement3)

// writes all prepared measurements with a single request into db.
boolean success = influx.write();
```

## Http client error codes
Internally `ESP8266HTTPClient` is used.
```C
/// HTTP client errors
#define HTTPC_ERROR_CONNECTION_REFUSED  (-1)
#define HTTPC_ERROR_SEND_HEADER_FAILED  (-2)
#define HTTPC_ERROR_SEND_PAYLOAD_FAILED (-3)
#define HTTPC_ERROR_NOT_CONNECTED       (-4)
#define HTTPC_ERROR_CONNECTION_LOST     (-5)
#define HTTPC_ERROR_NO_STREAM           (-6)
#define HTTPC_ERROR_NO_HTTP_SERVER      (-7)
#define HTTPC_ERROR_TOO_LESS_RAM        (-8)
#define HTTPC_ERROR_ENCODING            (-9)
#define HTTPC_ERROR_STREAM_WRITE        (-10)
#define HTTPC_ERROR_READ_TIMEOUT        (-11)
...
```
See [list of error codes](https://github.com/esp8266/Arduino/blob/cc0bfa04d401810ed3f5d7d01be6e88b9011997f/libraries/ESP8266HTTPClient/src/ESP8266HTTPClient.h#L44-L55) and [list of http status codes](https://github.com/esp8266/Arduino/blob/cc0bfa04d401810ed3f5d7d01be6e88b9011997f/libraries/ESP8266HTTPClient/src/ESP8266HTTPClient.h#L60-L120).

## Documentation
For the documentation see [html/class_influxdb.html](html/class_influxdb.html) (only works locally).
