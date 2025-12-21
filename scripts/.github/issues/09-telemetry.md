# Add Logging/Telemetry and Malformed Packet Counters
Labels: P2,enhancement
Assignees: @me

**Description**

Add counters for malformed Art-Net/sACN packets, socket errors, IGMP failures, and expose them via API or logs. Consider a `/api/metrics` endpoint for telemetry.

**Files / Areas**
- `components/mod_proto/*`
- `components/mod_web/*`

**Acceptance criteria**
- Counters present and increment under test.
- Exposed via `/api/metrics` or `/api/health`.

**Estimate**: 1-2d

**Checklist**
- [ ] Add counters and incremental updates
- [ ] Expose metrics via API
- [ ] Update docs and tests