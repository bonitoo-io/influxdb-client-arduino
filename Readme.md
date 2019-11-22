# InfluxDB v2 Arduino Client

Arduino client for writing and reading data from InfluxDB v2, local server or InfluxDB Cloud.Supports authentication, secure communication over TLS, batching and retrying.


## Batch writes
Client can write  data in batches, which is more officient. However, **if using batch writes, you new to create points with timestamps**. If points will not have timestamp assigned, InfluxDB will assign timestamp at the time of writing, which could be much later than the time of point creation.
Set batch sizes based on write cycles and updates requirement. 
E.g. If you would like to see updates on dashboard each minute and you are writing once per minute, batchsize should be 1 (or more depending of how many points do you write at once). In case your dashboard needs updates each hour and you are writing 3 points at once each minute, your batch size shoud be 180. Maximum recommended batchsize is 200.
