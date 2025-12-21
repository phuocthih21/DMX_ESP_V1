# Make `dmx_apply_new_timing()` Enforce Immediate Apply
Labels: P1,enhancement
Assignees: copilot AI

**Description**

`dmx_apply_new_timing()` currently exists for API compliance but does not force immediate driver updates. Implement immediate re-calculation and notify driver so timing changes take effect within a frame.

**Files / Areas**
- `components/mod_dmx/dmx_core.c`
- `components/mod_dmx/dmx_rmt.c`

**Acceptance criteria**
- Calling `dmx_apply_new_timing()` updates driver timing with low latency (<1 frame or documented limit).
- Verified with unit tests and hardware timing checks (oscilloscope / RMT verification).

**Estimate**: 1-2d

**Checklist**
- [ ] Implement immediate timing apply logic
- [ ] Add tests and hardware validation steps
- [ ] Document behavior and latency guarantees