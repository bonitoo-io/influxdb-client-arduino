# InfluxDB v2 Arduino Client

Simple Arduino client for writing and reading data from [InfluxDB 2](https://v2.docs.influxdata.com/v2.0/), local server or InfluxDB Cloud. Supports authentication, secure communication over TLS, batching and retrying.

## Basic Code
Using client is very easy. After you've [setup InfluxDB 2 server](https://v2.docs.influxdata.com/v2.0/get-started), first define connection parameters and client instance:
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

Next step is to add data. Single data row is reprented by the `Point` class. It consists of measurement name (like a table name), tags (which labels data) and fields (values to store):
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

And finally write data:
```cpp
// Write data
client.writePoint(pointDevice);
```

Now you will able to see data in the InfluxDB UI. You can use  `Data Explorer` or create a `Dashboard`.

Complete code is available in [BasicWrite example](examples/BasicWrite/BasicWrite.ino).

## Connecting to InfluxDB Cloud 2
Instead of seting up local InfluxDB 2 server, you can quickly [start with InfluxDB Cloud 2](https://v2.docs.influxdata.com/v2.0/cloud/get-started/) with [Free Plan](https://v2.docs.influxdata.com/v2.0/cloud/pricing-plans/#free-plan)

Connecting arduino client to InfuxDBCloud server requires few additional steps.
InfluxDBCloud uses secure communication (https) and we need to tell client to trust this connection.
Connection parameteres is almoost the same as above, only difference is that server url now points to the InfluxDB Cloud 2.0 where you got after subcription. You will find correct URL in  `InfluxDB UI -> Load Data -> Client Libraries`.


```cpp
// InfluxDB v2 server or cloud url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com(Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "influxdb-url"
// InfluxDB v2 server or cloud API authentication token (Use: InfluxDB UI -> Load Data -> Tokens -> <select token>)
#define INFLUXDB_TOKEN "token"
// InfluxDB v2 organization id (Use: InfluxDB UI -> Settings -> Profile -> <name under tile> )
#define INFLUXDB_ORG "org"
// InfluxDB v2 bucket name (Use: InfluxDB UI -> Load Data -> Buckets)
#define INFLUXDB_BUCKET "bucket"
```

In addition to the connection parameters we only need to tell a device correct timezone:
```cpp
// Timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Some values:
// Pacific Time: PST8PDT
// Eastern: EST5EDT
// Japanesse: JST-9
// Central Europe: CET-1CEST,M3.5.0,M10.5.0/3
#define TZ_INFO  "CET-1CEST,M3.5.0,M10.5.0/3"
```

We need to pass  additional parameter to client constructor, which is certificate of the server to trust. Constant `InfluxDbCloud2CACert` contains InfluxDB Cloud 2 CA certificate, which is predefined in this library: 
```cpp
// Single InfluxDB instance
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
```

Defining data and writing it to the db is the same:
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
Complete code is available in [SecureWrite example](examples/SecureWrite/SecureWrite.ino)

![Under Construction](res/under-construction.png "Image by Jose R. Cabello from Pixabay")


<!--
## Batch writes
Client can write  data in batches, which is more officient. However, **if using batch writes, you new to create points with timestamps**. If points will not have timestamp assigned, InfluxDB will assign timestamp at the time of writing, which could be much later than the time of point creation.
Set batch sizes based on write cycles and updates requirement. 
E.g. If you would like to see updates on dashboard each minute and you are writing once per minute, batchsize should be 1 (or more depending of how many points do you write at once). In case your dashboard needs updates each hour and you are writing 3 points at once each minute, your batch size shoud be 180. Maximum recommended batchsize is 200.
-->
