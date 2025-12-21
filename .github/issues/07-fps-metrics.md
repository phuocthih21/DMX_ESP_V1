# Add Accurate FPS/CPU Metrics & `/api/health`
Labels: P2,feature
Assignees: copilot AI

**Description**

Replace simplified FPS and CPU placeholders with accurate counters and expose `/api/health` or `/api/metrics`. Update WS messages to include accurate metrics.

**Files / Areas**
- `components/mod_web/src/mod_web_api.c`
- `components/mod_web/src/mod_web_ws.c`

**Acceptance criteria**
- Each DMX port reports FPS measured over a rolling window.
- CPU load sampling implemented (FreeRTOS stats or light-weight measurement).
- Health endpoint present and returns key metrics.

**Estimate**: 1-2d

**Checklist**
- [ ] Implement FPS counters per-port
- [ ] Implement CPU load sampling
- [ ] Add `/api/health` and WS metrics integration
- [ ] Add tests and docs