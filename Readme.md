# InfluxDB 2 Arduino Client

Simple Arduino client for writing and reading data from [InfluxDB 2](https://v2.docs.influxdata.com/v2.0/), it doens't matter whether a local server or InfluxDB Cloud. Supports authentication, secure communication over TLS, [batching](#writing_in_batches), [automatic retrying](#write_retrying) on server backpressure and connection faiure.

![Under Construction](res/under-construction.png "Image by Jose R. Cabello from Pixabay")

## Basic code
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

Next step is adding data. Single data row is represented by the `Point` class. It consists of measurement name (like a table name), tags (which labels data) and fields (values to store):
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

Now you will able to see data in the InfluxDB UI. You can use  `Data Explorer` or create a `Dashboard`.

Complete source code is available in [BasicWrite example](examples/BasicWrite/BasicWrite.ino).

## Connecting to InfluxDB Cloud 2
Instead of seting up local InfluxDB 2 server, you can quickly [start with InfluxDB Cloud 2](https://v2.docs.influxdata.com/v2.0/cloud/get-started/) with [Free Plan](https://v2.docs.influxdata.com/v2.0/cloud/pricing-plans/#free-plan).

Connecting arduino client to InfuxDB Cloud server requires few additional steps.
InfluxDBCloud uses secure communication (https) and we need to tell client to trust this connection.
Connection parameters are almoost the same as above, only difference is that server url now points to the InfluxDB Cloud 2, where you've got after you've finished creating InfluxDB Cloud 2 subcription. You will find correct server URL in  `InfluxDB UI -> Load Data -> Client Libraries`.
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

We need to pass  additional parameter to client constructor, which is certificate of the server to trust. Constant `InfluxDbCloud2CACert` contains InfluxDB Cloud 2 CA certificate, which is predefined in this library: 
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

Defining data and writing it to the db is the same as in case of [BasicWrite](#basic-code):
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
InfluxDB client for Arduino can write data in batches. A batch is simply a set of points that will be sent at once. To create a batch, client will keep all points until the number of points reaches the batch size and then it will write all points at once to the InfluDB server. This is often more efficient than writing each point separately. 

### Timestamp
If using batch writes, timestamp should be employed. Timestamp specifies the time where data was gathered and it is used in the form of number of seconds (milliseconds, etc) from epoch (1.1.1970) UTC.
If points have no timestamp assigned, InfluxDB assigns timestamp at the time of writing, which could happen much later than the data has been obtained, because final batch write will happen when the batch is full (or when [flush buffer](#buffer_handling) is forced).

InfuxDB allows to send timestamp in various precisions - nanoseconds, microseconds, milliseconds or seconds. The miliseconds precision is usually enough for using on Arduino,

Client  has to be configured which precision to use. Default is not using timestamp. `setWriteOptions` methods allows setting various parameters and one of them is __write precision__:
``` cpp
// Set write precision to milliseconds. Leave other parameters default.
client.setWriteOptions(WritePrecision::MS);
```
When a write precision is configured, client will automatically assign current time to timestamp of each written point, which doesn't have a timestamp assigned. 

If you want to manage timestamp on your own, there are several ways how to set timestamp explicitly.
- `setTime(WritePrecision writePrecision)` - Sets timestamp to actual time in desired precision
- `setTime(unsigned long seconds)` -  Sets timestamp in seconds since epoch. Write precision must be set to `S` 
- `setTime(String timestamp)` - Set custom timestamp in precision specified in InfluxDBClient. 


### Configure Time
Dealing with timestamp requires the device has correctly set time. This can be done with just a few lines of code:
```cpp
// Synchronize UTC time with NTP servers
// Accurate time is necessary for certificate validaton and writing in batches
configTime(0, 0, "pool.ntp.org", "time.nis.gov");
// Set timezone
setenv("TZ", "PST8PDT", 1);
```
`configTime` method starts the time synchronization with NTP servers. First two parameters specifies DST and timezone offset, but we keep them zero and configure timezone info later.
Last two string parameters are internet address of NTP servers. Check [pool.ntp.org](https://www.pool.ntp.org/zone) for adress of some local NTP servers.

Using `setenv` with `TZ` param ensures a device has correct timezone. This is critical for distinguishing UTC and a local timezone, because timestamps of points must be set in UTC timezone.
Second parameter is timezone information, which is described at [https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html). 

Values for some timezones:
- Central Europe: `CET-1CEST,M3.5.0,M10.5.0/3`
- Eastern: `EST5EDT`
- Japanesse: `JST-9`
- Pacific Time: `PST8PDT`

We could set the timezone info (DST and UTC offset) also in the first two parameters of `configTime`, but there is a [bug on ESP8266](TODO: link) which causes a time behaves as it is in UTC, even UTC offset was specified.

### Batch Size
Setting batch size depends on data gathering and db updating strategy.

If data is written in short periods (seconds), batch size should be according to expected write periods and update frequency requirement. 
For example if you would like to see updates (on dashboard or in procesing) each minute and you are measuring single data (1 point) each 10s (6 points per minute), batchsize should be 6. In case it is enough to update each hour and you are creating 1 point at once each minute, your batch size shoud be 60. Maximum recommended batchsize is 200. Depends on the RAM of the device (80KB for ESP8266 and 512KB for ESP32).

In case data should written in longer periods and gathered data consists of several points batch size should be set to expected number of points.

To set batch size we use [setWriteOptions](#write_options) method, where second parameter controls batch size:
```cpp
// Enable messages batching
client.setWriteOptions(WritePrecision::MS, 10);
```
Writing point will add point to the underlying buffer until the batch size is reached:
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

In case of number of points is not always same, set batch size to maximum number of points and use `flushBuffer()` method to force writing to db. See [Buffer Handling](#buffer_handling_and_retrying) for more details.

## Buffer Handling and Retrying
InfluxDB contains underlying buffer for handling writing in batches and automatic retrying on server backpressure and connection failure.

It's size is controled by the 3rd parameter of [setWriteOptions](#write_options) method:
```cpp
// Enable messages batching
client.setWriteOptions(WritePrecision::MS, 10, 30);
```
The third parameter specifies buffer size. Recommended size is at least 2 x batch size. 

State of the buffer can be determined via two methods:
 - `isBufferEmpty()` - Returns true if buffer is empty
 - `isBufferFull()` - Returns true if buffer is full
 
 Full buffer can occur when there is a problem with internet connection or InfluxDB server is overloaded. In such cases, points to write remains in buffer. When more points are added and connection problem remains, buffer will reach top and new points will overwrite older points.

 Each attemp to write a point will try to send older points in buffer. So,`isBufferFull()` method can be used to skip low priority points.

 `flushBuffer()` can be used to force writing, even number of points in buffer is lower than batch size. With help of `isBufferEmpty()` method a check can be made before a device go to sleep:

 ```cpp
  // Check whether buffer in not empty
  if (!client.isBufferEmpty()) {
      // Write all remaining points to db
      client.flushBuffer();
  }
```

Other methods for dealing with buffer:
 - `checkBuffer()` - Checks points buffer status and flushes if number of points reached batch size or flush interval runs out. This main method for controling buffer and it is used internally.
 - `resetBuffer()` - Clears the buffer.

Check [SecureBatchWrite example](examples/SecureBatchWrite/SecureBatchWrite.ino) for example code of buffer handling functions.

## Write Options
Writing points can be controller via several parameters in `setWriteOptions` method:

| Parameter | Default Value | Meaning |
|-----------|---------------|---------| 
| precision | `WritePrecision::NoTime` | Timestamp precision of written data |
| batchSize | `1` | Number of points that will be written to the databases at once |
| bufferSize | `5` | Maximum number of points in buffer. Buffer contains new data that will be written to the database and also data that failed to be written due to network failure or server overloading |
| flushInterval | `60` | Maximum time(in seconds) data will be held in buffer before are written to the db |
| preserveConnection | `false` | true if underlying HTTP connection should be kept open |

## Querying
InfluxDB uses [Flux](https://www.influxdata.com/products/flux/) to process and query data. InfluxDB client for Arduino offers simple way how to query data with `query` function:
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
This library also provides couple of helper methods for parsing such result set.

If query results in empty result set, server returns empty response. As empty result returned from the `query` function indicates an error,
use `wasLastQuerySuccessful()` method to determine final status.

Complete source code is available in [Query example](examples/Query/Query.ino).
