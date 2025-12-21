# Sample Output

## /api/sys/info

```json
{
  "uptime": 12345,
  "cpu_load": 5,
  "free_heap": 61440,
  "eth_up": true,
  "wifi_up": false
}
```

## /api/dmx/status

```json
[
  { "port": 0, "universe": 1, "enabled": true, "fps": 30 },
  { "port": 1, "universe": 2, "enabled": false, "fps": 30 }
]
```

## WebSocket message - snapshot

```json
{
  "type": "SNAPSHOT",
  "timestamp": 1700000000,
  "payload": {
    "uptime": 12346,
    "cpu_load": 6,
    "free_heap": 61440,
    "eth_up": true,
    "wifi_up": false,
    "dmx": [ ... ]
  }
}
```

## WebSocket message - event

```json
{
  "type": "EVENT",
  "event": "SYS_EVT_LINK_DOWN",
  "timestamp": 1700000002,
  "payload": { "link": "eth", "up": false }
}
```
