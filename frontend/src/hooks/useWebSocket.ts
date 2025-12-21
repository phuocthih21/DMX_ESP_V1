/**
 * WebSocket client hook with auto-reconnect and fallback to polling
 */

import { useEffect, useRef, useState } from 'react';
import { getWebSocketURL } from '../utils/deviceDiscovery';
import { handleWSMessage } from '../utils/wsMessageHandler';
import { useConnectionStore } from '../stores/connectionStore';
import { WS_RECONNECT_DELAY_INITIAL, WS_RECONNECT_DELAY_MAX, WS_RECONNECT_BACKOFF_MULTIPLIER } from '../utils/constants';

export interface UseWebSocketOptions {
  enabled?: boolean;
  onConnect?: () => void;
  onDisconnect?: () => void;
  onError?: (error: Event) => void;
}

/**
 * WebSocket hook with auto-reconnect
 */
export function useWebSocket({ enabled = true, onConnect, onDisconnect, onError }: UseWebSocketOptions = {}): {
  connected: boolean;
  reconnect: () => void;
} {
  const [connected, setConnected] = useState(false);
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimeoutRef = useRef<number | null>(null);
  const reconnectDelayRef = useRef(WS_RECONNECT_DELAY_INITIAL);
  const setWSConnected = useConnectionStore((state) => state.setWSConnected);
  const online = useConnectionStore((state) => state.online);

  const connect = () => {
    if (!enabled || !online) {
      return;
    }

    try {
      const url = getWebSocketURL();
      if (!url) {
        console.warn('[WebSocket] No URL available for connection');
        return;
      }
      const ws = new WebSocket(url);

      ws.onopen = () => {
        console.log('[WebSocket] Connected');
        setConnected(true);
        setWSConnected(true);
        reconnectDelayRef.current = WS_RECONNECT_DELAY_INITIAL; // Reset delay on success
        onConnect?.();
      };

      ws.onmessage = (event) => {
        try {
          const message = JSON.parse(event.data);
          handleWSMessage(message);
        } catch (error) {
          console.error('[WebSocket] Failed to parse message:', error);
        }
      };

      ws.onerror = (error) => {
        console.error('[WebSocket] Error:', error);
        // Don't crash the app on WebSocket errors
        onError?.(error);
      };

      ws.onclose = () => {
        console.log('[WebSocket] Disconnected');
        setConnected(false);
        setWSConnected(false);
        onDisconnect?.();

        // Auto-reconnect if enabled and online
        if (enabled && online) {
          const delay = reconnectDelayRef.current;
          reconnectDelayRef.current = Math.min(
            reconnectDelayRef.current * WS_RECONNECT_BACKOFF_MULTIPLIER,
            WS_RECONNECT_DELAY_MAX
          );

          reconnectTimeoutRef.current = window.setTimeout(() => {
            connect();
          }, delay) as unknown as number;
        }
      };

      wsRef.current = ws;
    } catch (error) {
      console.error('[WebSocket] Failed to connect:', error);
      // Silently handle connection errors - don't crash the app
      onError?.(error as Event);
      // Schedule reconnect attempt
      if (enabled && online) {
        const delay = reconnectDelayRef.current;
        reconnectDelayRef.current = Math.min(
          reconnectDelayRef.current * WS_RECONNECT_BACKOFF_MULTIPLIER,
          WS_RECONNECT_DELAY_MAX
        );
        reconnectTimeoutRef.current = window.setTimeout(() => {
          connect();
        }, delay) as unknown as number;
      }
    }
  };

  const disconnect = () => {
    if (wsRef.current) {
      wsRef.current.close();
      wsRef.current = null;
    }
    if (reconnectTimeoutRef.current) {
      clearTimeout(reconnectTimeoutRef.current);
      reconnectTimeoutRef.current = null;
    }
    setConnected(false);
    setWSConnected(false);
  };

  const reconnect = () => {
    disconnect();
    reconnectDelayRef.current = WS_RECONNECT_DELAY_INITIAL;
    connect();
  };

  useEffect(() => {
    // Add small delay to prevent immediate connection attempts
    const timeoutId = setTimeout(() => {
      if (enabled && online) {
        connect();
      } else {
        disconnect();
      }
    }, 100);

    return () => {
      clearTimeout(timeoutId);
      disconnect();
    };
  }, [enabled, online]);

  return { connected, reconnect };
}

