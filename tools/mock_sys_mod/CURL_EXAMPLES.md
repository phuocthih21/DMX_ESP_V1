# CURL Examples

GET system info:

```bash
curl http://localhost:3000/api/sys/info
```

GET dmx status:

```bash
curl http://localhost:3000/api/dmx/status
```

GET network status:

```bash
curl http://localhost:3000/api/network/status
```

Note: WebSocket testing requires a WS client (see `node test_client.js` or use `wscat`).
