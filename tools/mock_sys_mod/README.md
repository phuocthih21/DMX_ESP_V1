Mock SYS_MOD server

Requirements:
- Node.js 16+ (LTS)

Install:

```bash
cd tools/mock_sys_mod
npm install
```

Run:

```bash
npm start
```

Endpoints:
- GET http://localhost:3000/api/sys/info
- GET http://localhost:3000/api/dmx/status
- GET http://localhost:3000/api/network/status
- WS  ws://localhost:3000/ws/status

Sample curl:

```bash
curl http://localhost:3000/api/sys/info
```

WebSocket sample (node):

```js
const WebSocket = require('ws');
const ws = new WebSocket('ws://localhost:3000/ws/status');
ws.on('message', (m) => console.log('msg', m.toString()));
```

Run automated quick test (after `npm start` in another terminal):

```bash
node test_client.js
```
