MOD_NET component

This component implements an Ethernet-first network manager for DMX Node.

Files:
- include/net_types.h  : public data types
- include/mod_net.h    : public API
- net_eth.c            : W5500 SPI Ethernet driver
- net_wifi.c           : WiFi STA/AP management
- net_mdns.c           : mDNS responder
- mod_net.c            : central logic (event handlers & state machine)

Status: initial implementation with stubs and basic event handling. See `DESIGN_MOD/MOD_NET.md` for full design.

TODO:
- Wire Kconfig options to pin macros (done: Kconfig placeholders added)
- Implement static IP handling (esp_netif_set_ip_info) (implemented)
- Set W5500 MAC from ESP base MAC (TODO)

Manual test plan:
1. Hot-plug Ethernet
   - Connect W5500 to network -> expect Ethernet link up and DHCP IP (or static IP if configured). Observe Status LED switching to Ethernet.
   - Unplug Ethernet -> expect device to connect to configured WiFi STA and obtain IP.
2. WiFi fallback -> Wrong password
   - Configure wrong WiFi password -> after 3 failed retries, device should start WiFi AP for rescue mode.
3. mDNS discovery
   - With device connected, ping <hostname>.local and verify response. Browse to http://<hostname>.local.

Notes:
- Use `menuconfig` to configure MOD_NET Kconfig options if needed.
- The W5500 MAC derivation and setting is TODO; it is recommended to derive MAC from ESP base MAC to avoid duplicates on the LAN.
