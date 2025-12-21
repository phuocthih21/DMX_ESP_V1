const express = require('express');
const WebSocket = require('ws');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(express.json());

let uptime = 12345;
let cpu_load = 5;
let free_heap = 60 * 1024;
let eth_up = true;
let wifi_up = false;

let dmx_ports = [
  { port: 0, universe: 1, enabled: true, fps: 30 },
  { port: 1, universe: 2, enabled: false, fps: 30 },
  { port: 2, universe: 3, enabled: false, fps: 25 },
  { port: 3, universe: 4, enabled: true, fps: 60 },
];

let network_status = {
  eth_up: eth_up,
  wifi_up: wifi_up,
  eth_ip: '192.168.1.100',
  wifi_ip: null
};

app.get('/api/sys/info', (req, res) => {
  res.json({
    uptime,
    cpu_load,
    free_heap,
    eth_up,
    wifi_up
  });
});

app.get('/api/dmx/status', (req, res) => {
  res.json(dmx_ports);
});

app.get('/api/network/status', (req, res) => {
  res.json(network_status);
});

const server = app.listen(3000, () => {
  console.log('Mock SYS_MOD server listening on http://localhost:3000');
  console.log('WS endpoint: ws://localhost:3000/ws/status');
});

const wss = new WebSocket.Server({ server, path: '/ws/status' });

function make_status_snapshot() {
  return {
    type: 'SNAPSHOT',
    timestamp: Math.floor(Date.now()/1000),
    payload: {
      uptime,
      cpu_load,
      free_heap,
      eth_up,
      wifi_up,
      dmx: dmx_ports
    }
  };
}

function make_event(evt_type, payload) {
  return {
    type: 'EVENT',
    event: evt_type,
    timestamp: Math.floor(Date.now()/1000),
    payload: payload || {}
  };
}

wss.on('connection', (ws) => {
  console.log('WS client connected');
  // send initial snapshot
  ws.send(JSON.stringify(make_status_snapshot()));

  const interval = setInterval(() => {
    // simulate state changes
    uptime += 1;
    cpu_load = (cpu_load + 1) % 100;
    // occasionally flip link
    if (Math.random() < 0.02) {
      eth_up = !eth_up;
      network_status.eth_up = eth_up;
      const evt = make_event(eth_up ? 'SYS_EVT_LINK_UP' : 'SYS_EVT_LINK_DOWN', { link: 'eth', up: eth_up });
      ws.send(JSON.stringify(evt));
    }

    // periodic SNAPSHOT
    ws.send(JSON.stringify(make_status_snapshot()));
  }, 1000);

  ws.on('close', () => {
    console.log('WS client disconnected');
    clearInterval(interval);
  });

  ws.on('message', (msg) => {
    console.log('WS recv:', msg.toString());
    // Support minimal control messages for testing
    try {
      const j = JSON.parse(msg);
      if (j.cmd === 'ping') ws.send(JSON.stringify({ type: 'PONG', timestamp: Math.floor(Date.now()/1000) }));
    } catch (e) {}
  });
});

// Simple CLI: allow toggling values
process.on('SIGINT', () => {
  console.log('Shutting down mock server');
  server.close(() => process.exit(0));
});
