# Add HTTPS/TLS Support or Document Reverse-Proxy Setup
Labels: P1,feature
Assignees: @me

**Description**

Add TLS support to the web interface or provide clear documentation and scripts to deploy the device behind an HTTPS reverse-proxy for secure management.

**Files / Areas**
- `components/mod_web/*`
- Ops documentation (README / deployment docs)

**Acceptance criteria**
- Either TLS termination is supported on-device (with cert management) or a documented recommended reverse-proxy setup is provided.
- Tests validate access via https and that sensitive endpoints are protected.

**Estimate**: 2-5d

**Checklist**
- [ ] Choose TLS approach (on-device vs proxy)
- [ ] Implement or document setup
- [ ] Test secure access workflows