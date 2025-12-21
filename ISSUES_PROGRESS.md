# ISSUES_PROGRESS.md

**Repository:** phuocthih21/DMX_ESP_V1
**Generated:** 2025-12-22

## ✅ Summary (high level)
- Found 14 open issues in the repository.
- Several issues are already implemented and covered by unit tests; others remain and are planned.

---

## 🔍 Issue status (short)
| # | Title | Status | Notes |
|---:|---|---|---|
| 1, 6 | Implement sACN Priority Handling in Merge Engine | ✅ Implemented & tested | `components/mod_proto/merge.c` contains priority logic; tests: `components/mod_proto/test/unit_test/main/test_proto.c` (`test_priority_override`, `test_priority_override_ltp`). Ready to close. |
| 5 | Implement sACN IGMP Join/Leave & `proto_reload_config` | ✅ Implemented & tested | Implemented in `components/mod_proto/sacn.c` and `components/mod_proto/proto_mgr.c` (`sacn_join_universe`, `sacn_apply_memberships_for_socket`, `proto_reload_config`). Unit test: `test_proto_reload_join_leave`. |
| 4, 13, 11 | Add Logging/Telemetry and Accurate FPS/CPU metrics / `/api/health` | ⚠️ Not implemented | No `/api/health` or `/api/metrics` endpoints found; need to design metrics counters and API. Frontend has `SystemMetrics` component expecting metrics. |
| 7 | Add Authentication & Protect Sensitive Web API Endpoints | ✅ Implemented (partially) | Authentication subsystem implemented in `components/mod_web/mod_web_auth.*` (NVS-hashed password, bearer tokens). Protected admin endpoints now include `/api/sys/reboot`, `/api/sys/factory`, `/api/net/config`, and `/api/dmx/config`. Added unit tests for auth helpers. |
| 2, 9 | Implement Static IP Apply & Robust `net_reload_config()` | ⚠️ Partially implemented | `net_eth.c` and `net_wifi.c` apply static IP when interfaces start, but `net_reload_config()` TODO needs implementing to apply static IP to running netifs and to handle interface switching smoothly. |
| 3, 10 | Make `dmx_apply_new_timing()` Enforce Immediate Apply | ⚠️ Not implemented | `components/mod_dmx/dmx_core.c` currently documents immediate apply as TODO; implementation needed to force driver update in-frame. |
| 12 | Expand Unit/Integration Tests & Add CI | ⚠️ Not implemented | Tests exist for some features; more integration/stress tests and GitHub Actions CI are planned. |
| 8 | Add HTTPS/TLS support or document reverse-proxy | ⚠️ Not implemented | No TLS on-device or guidance found; needs design decision (on-device vs proxy). |

---

## ✅ What I verified / evidence
- sACN priority behavior: `merge.c` compares `source_a.priority` and `source_b.priority` and selects the higher-priority source for the whole universe; tests present in `test_proto.c`.
- IGMP join/leave: `sacn.c` has `sacn_socket_join`/`sacn_socket_leave` and handles queued joins when socket becomes available; `proto_mgr.c` `proto_reload_config()` computes diffs.

## 🔧 Next steps (proposed priority)
1. (High) Implement Authentication middleware & protect sensitive endpoints (issue #7). Add tests and frontend integration notes. ✅ Start next.  
2. (High) Implement `/api/health` and telemetry counters (malformed packets, socket errors, IGMP failures, FPS, CPU); expose via endpoint / WS updates (issues #4, #11, #13).  
3. (Medium) Implement `dmx_apply_new_timing()` immediate apply logic and add tests (issues #3, #10).  
4. (Medium) Improve `net_reload_config()` to apply static IP to active netifs and better interface fallback tests (issues #2, #9).  
5. (Lower) Add CI workflow and expand tests (issue #12).  
6. (Optional) Decide TLS approach and provide on-device or reverse-proxy docs (issue #8).

---

## 📋 Reporting / Next actions I'll take now
- Create a PR for each implemented fix and close issues that are already implemented (1/6 and 5) — pending your confirmation or review.  
- Start implementing **Authentication** (#7) next; I'll open a branch and add an incremental PR with middleware + tests.  

---

If this plan looks good, confirm and I will start with issue #7 (Auth). I will update this file (and create PRs) as I complete each item.
