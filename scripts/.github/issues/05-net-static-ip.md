# Implement Static IP Apply & Robust `net_reload_config`
Labels: P1,bug
Assignees: @me

**Description**

`net_reload_config()` must apply static IP when DHCP is disabled, and the network manager should handle interface switching (Ethernet -> WiFi -> AP) per spec.

**Files / Areas**
- `components/mod_net/mod_net.c`

**Acceptance criteria**
- Static IP settings are applied to netif when DHCP disabled.
- Tests validate smooth auto-switching behavior and event handling (NET_EVENT_GOT_IP, LOST_IP).

**Estimate**: 2-3d

**Checklist**
- [ ] Implement static IP apply logic
- [ ] Add tests for auto-switching flows
- [ ] Update docs and config validation