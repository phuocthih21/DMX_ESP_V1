const http = require('http');
const WebSocket = require('ws');

function get(path, cb) {
  http.get({ hostname: 'localhost', port: 3000, path, agent: false }, (res) => {
    let data = '';
    res.on('data', (chunk) => data += chunk);
    res.on('end', () => cb(null, data));
  }).on('error', (e) => cb(e));
}

get('/api/sys/info', (err, data) => {
  if (err) return console.error('GET error:', err);
  console.log('/api/sys/info ->', data);
});

get('/api/dmx/status', (err, data) => {
  if (err) return console.error('GET error:', err);
  console.log('/api/dmx/status ->', data);
});

const ws = new WebSocket('ws://localhost:3000/ws/status');
ws.on('open', () => console.log('WS connected'));
ws.on('message', (m) => {
  console.log('WS message:', m.toString());
});
ws.on('close', () => console.log('WS closed'));

// Close after 6 seconds
setTimeout(() => { ws.close(); process.exit(0); }, 6000);
