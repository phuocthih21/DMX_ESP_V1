# Add Authentication & Protect Sensitive Web API Endpoints
Labels: P0,security
Assignees: @me

**Description**

Protect sensitive REST endpoints (e.g., `/api/sys/reboot`, `/api/sys/factory`, `/api/net/config`) using token-based auth or other suitable method. Add minimal role-based checks so only admins can execute destructive operations.

**Files / Areas**
- `components/mod_web/src/mod_web_api.c`
- `components/mod_web/*` auth utilities
- `components/sys_mod` to store hashed credentials securely

**Acceptance criteria**
- Admin-only endpoints require authentication; unauthorized calls return 401.
- Provide a plan for storing secrets (NVS + hashing or secure element).  
- Add tests or scripts to validate authentication integration with frontend.

**Estimate**: 2-4d

**Checklist**
- [ ] Design auth scheme (token or basic+hash)
- [ ] Implement authentication middleware in web server
- [ ] Protect sensitive APIs and add tests
- [ ] Update frontend to support auth flow (or document required changes)