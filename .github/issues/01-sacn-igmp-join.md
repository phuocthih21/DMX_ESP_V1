# Implement sACN IGMP Join/Leave & proto_reload_config
Labels: P0,feature
Assignees: copilot AI

**Description**

When universe routing changes, the firmware must perform real IGMP join/leave on the sACN multicast group (239.255.u.u). Currently `sacn_join_universe()` only logs and `proto_reload_config()` is a no-op.

**Files / Areas**
- `components/mod_proto/sacn.c`
- `components/mod_proto/proto_mgr.c`
- Potentially `components/mod_net` (socket sharing / interface selection)

**Acceptance criteria**
- Implement IGMP join/leave on the sACN socket so that the device actually subscribes to multicast groups.
- Implement `proto_reload_config()` to compute diffs and perform joins/leaves for newly mapped/unmapped universes.
- Unit/integration test validates join/leave occurs when mapping changes (mock or integration environment).  

**Estimate**: 2-3d

**Checklist**
- [ ] Implement IGMP join logic in `sacn_join_universe()` (and a `sacn_leave_universe()`)
- [ ] Ensure sACN socket supports membership changes (handle errors & log)
- [ ] Implement `proto_reload_config()` to apply diff join/leave
- [ ] Add tests (unit or integration) that validate join/leave behavior
- [ ] Document behavior in `MOD_PROTO.md`