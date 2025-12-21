/**
 * WebSocket message parser and state updater
 */

import { WSMessage, WSSystemStatus, WSDMXPortStatus, WSNetworkLink, WSSystemEvent } from '../api/types';
import { useSystemStore } from '../stores/systemStore';
import { useDMXStore } from '../stores/dmxStore';
import { useNetworkStore } from '../stores/networkStore';

/**
 * Handle WebSocket message and update stores
 * Supports both standard format and mock server format
 */
export function handleWSMessage(message: WSMessage | any): void {
  // Handle mock server format (SNAPSHOT/EVENT)
  if (message.type === 'SNAPSHOT' && message.payload) {
    const payload = message.payload;
    
    // Update system info
    if (payload.uptime !== undefined) {
      useSystemStore.getState().setInfo({
        uptime: payload.uptime,
        cpu_load: payload.cpu_load,
        free_heap: payload.free_heap,
        eth_up: payload.eth_up,
        wifi_up: payload.wifi_up,
      });
    }
    
    // Update DMX ports
    if (Array.isArray(payload.dmx)) {
      useDMXStore.getState().setPorts(payload.dmx);
    }
    
    // Update network status
    if (payload.eth_up !== undefined || payload.wifi_up !== undefined) {
      useNetworkStore.getState().setStatus({
        eth_up: payload.eth_up || false,
        wifi_up: payload.wifi_up || false,
        eth_ip: payload.eth_ip || null,
        wifi_ip: payload.wifi_ip || null,
      });
    }
    return;
  }

  // Handle mock server EVENT format
  if (message.type === 'EVENT' && message.event) {
    const event = message.event;
    const payload = message.payload || {};
    
    if (event === 'SYS_EVT_LINK_UP' || event === 'SYS_EVT_LINK_DOWN') {
      const networkStore = useNetworkStore.getState();
      const currentStatus = networkStore.status;
      if (currentStatus && payload.link) {
        if (payload.link === 'eth') {
          networkStore.setStatus({
            ...currentStatus,
            eth_up: payload.up || event === 'SYS_EVT_LINK_UP',
          });
        } else if (payload.link === 'wifi') {
          networkStore.setStatus({
            ...currentStatus,
            wifi_up: payload.up || event === 'SYS_EVT_LINK_UP',
          });
        }
      }
    }
    return;
  }

  // Handle standard format
  switch (message.type) {
    case 'system.status': {
      const data = (message as WSSystemStatus).data;
      useSystemStore.getState().setInfo({
        uptime: data.uptime,
        cpu_load: data.cpu,
        free_heap: data.heap,
      });
      break;
    }

    case 'dmx.port_status': {
      const data = (message as WSDMXPortStatus).data;
      useDMXStore.getState().updatePort(data.port, {
        universe: data.universe,
        enabled: data.enabled,
        fps: data.fps,
      });
      break;
    }

    case 'network.link': {
      const data = (message as WSNetworkLink).data;
      const networkStore = useNetworkStore.getState();
      const currentStatus = networkStore.status;
      if (currentStatus) {
        if (data.iface === 'eth') {
          networkStore.setStatus({
            ...currentStatus,
            eth_up: data.status === 'up',
          });
        } else if (data.iface === 'wifi') {
          networkStore.setStatus({
            ...currentStatus,
            wifi_up: data.status === 'up',
          });
        }
      }
      break;
    }

    case 'system.event': {
      const data = (message as WSSystemEvent).data;
      // Log system events (can be extended to show notifications)
      console.log(`[System Event] ${data.code} (${data.level})`);
      break;
    }

    default:
      console.warn('Unknown WebSocket message type:', message.type);
  }
}

