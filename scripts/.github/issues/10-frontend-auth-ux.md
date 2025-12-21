# Frontend: Add Auth UX & Align Endpoints
Labels: P2,feature
Assignees: @me

**Description**

Update the frontend to support authentication (token storage, login flow) and ensure it calls protected endpoints correctly. Align API endpoints and error handling with backend changes.

**Files / Areas**
- `frontend/src/*` (`api/client.ts`, auth components, update UI)`

**Acceptance criteria**
- Frontend supports login and stores token securely (in-memory / secure storage recommendation).
- Frontend calls protected endpoints with token and handles 401/403 gracefully.

**Estimate**: 1-3d

**Checklist**
- [ ] Add login component and auth client
- [ ] Update API client to send auth header
- [ ] Update UI flows and error handling
- [ ] Add E2E or integration tests (optional)