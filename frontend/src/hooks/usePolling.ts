/**
 * Custom hook for REST API polling with configurable intervals
 */

import { useEffect, useRef } from 'react';
import { useConnectionStore } from '../stores/connectionStore';

export interface UsePollingOptions {
  enabled?: boolean;
  interval: number;
  onPoll: () => Promise<void>;
  onError?: (error: Error) => void;
}

/**
 * Poll API endpoint at specified interval
 * Automatically pauses when WebSocket is connected or when disabled
 */
export function usePolling({ enabled = true, interval, onPoll, onError }: UsePollingOptions): void {
  const wsConnected = useConnectionStore((state) => state.wsConnected);
  const online = useConnectionStore((state) => state.online);
  const intervalRef = useRef<number | null>(null);
  const timeoutRef = useRef<number | null>(null);
  const retryCountRef = useRef(0);

  useEffect(() => {
    // Don't poll if WebSocket is connected, offline, or disabled
    if (!enabled || wsConnected || !online) {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
        intervalRef.current = null;
      }
      return;
    }

    let isMounted = true;

    const poll = async () => {
      if (!isMounted) return;

      try {
        await onPoll();
        retryCountRef.current = 0; // Reset retry count on success
      } catch (error) {
        if (!isMounted) return;

        retryCountRef.current += 1;
        const retryDelay = Math.min(1000 * Math.pow(2, retryCountRef.current), 10000); // Exponential backoff, max 10s

        if (onError) {
          onError(error as Error);
        }

        // Retry after delay
        if (timeoutRef.current) {
          clearTimeout(timeoutRef.current);
        }
        timeoutRef.current = window.setTimeout(() => {
          if (isMounted && enabled && !wsConnected && online) {
            poll();
          }
        }, retryDelay);
      }
    };

    // Initial poll
    poll();

    // Set up interval
    intervalRef.current = window.setInterval(() => {
      if (isMounted && enabled && !wsConnected && online) {
        poll();
      }
    }, interval) as unknown as number;

    return () => {
      isMounted = false;
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
        intervalRef.current = null;
      }
      if (timeoutRef.current) {
        clearTimeout(timeoutRef.current);
        timeoutRef.current = null;
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [enabled, interval, wsConnected, online]); // onPoll and onError are stable callbacks
}

