MOD_DMX

This component implements the DMX512 output engine.

- Ports A/B (RMT)
- Ports C/D (UART RS485)

Files:
- include/mod_dmx.h
- dmx_core.c
- dmx_rmt.c
- dmx_uart.c

Notes:
- Critical functions are placed IRAM (`IRAM_ATTR`) to prevent cache misses.
- Task is pinned to core 1 at high priority.
