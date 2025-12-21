# Expand Unit / Integration Tests & Add CI
Labels: P2,testing
Assignees: @me

**Description**

Add tests covering: sACN multicast join/leave, sACN priority, net reload static IP, web API auth flows, and a stress test for 16 universes at 44Hz. Set up CI (GitHub Actions) to run unit tests and build.

**Files / Areas**
- `components/*/test` (new tests)
- `.github/workflows/ci.yml`

**Acceptance criteria**
- Tests exist and run in CI with pass/fail reporting.
- Stress test results and test harness documented.

**Estimate**: 3-7d

**Checklist**
- [ ] Add unit tests for new protocol behaviors
- [ ] Add integration tests (CI job may be optional for full integration)
- [ ] Add GitHub Actions workflow to run unit tests and builds
- [ ] Add test documentation and run instructions