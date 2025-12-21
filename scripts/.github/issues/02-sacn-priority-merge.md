# Implement sACN Priority Handling in Merge Engine
Labels: P0,bug
Assignees: @me

**Description**

The sACN parser extracts the `priority` field but the merge engine does not use it to decide which source should win. Per spec, the highest priority source should override lower priorities.

**Files / Areas**
- `components/mod_proto/merge.c`
- `components/mod_proto/*` tests

**Acceptance criteria**
- Merge engine honors sACN priority per FR-PROTO-05.
- Unit tests added for priority override scenarios and timeout revert behavior.

**Estimate**: 1-2d

**Checklist**
- [ ] Update merge logic to consider `priority` (both HTP & LTP semantics)
- [ ] Add unit tests for priority cases
- [ ] Add documentation changes to `MOD_PROTO.md`