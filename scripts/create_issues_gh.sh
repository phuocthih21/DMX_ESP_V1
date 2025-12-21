#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   export GITHUB_REPO="phuocthih21/DMX_ESP_V1"
#   export GH_TOKEN="<your token>"  # or authenticate via `gh auth login`
#   ./scripts/create_issues_gh.sh

REPO="${GITHUB_REPO:-}":
if [ -z "$REPO" ]; then
  echo "ERROR: Please set GITHUB_REPO environment variable (owner/repo)." >&2
  exit 1
fi

# Iterate over markdown files in .github/issues
for f in .github/issues/*.md; do
  echo "Processing $f"
  title=$(sed -n '1s/^# \+//p' "$f")
  labels=$(grep -m1 '^Labels:' "$f" | sed 's/^Labels:[[:space:]]*//') || labels=""
  assignees=$(grep -m1 '^Assignees:' "$f" | sed 's/^Assignees:[[:space:]]*//') || assignees=""
  # Build body: skip metadata lines (title, Labels, Assignees)
  body=$(awk 'NR>1 && !/^Labels:/ && !/^Assignees:/{print}' "$f")

  echo "Creating issue: $title"
  gh issue create --repo "$REPO" --title "$title" --body "$body" $( [ -n "$labels" ] && printf -- "--label '%s'" "$labels" ) $( [ -n "$assignees" ] && printf -- "--assignee '%s'" "$assignees" ) || {
    echo "Failed to create issue: $title" >&2
  }
  sleep 0.5
done

echo "Completed creating issues. Verify on GitHub."