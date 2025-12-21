# Creating GitHub Issues (Local)

This folder contains issue markdowns and a `gh`-based script to create issues in your repository.

Prerequisites:
- `gh` CLI installed and authenticated (`gh auth login`) OR export `GH_TOKEN`.
- Set `GITHUB_REPO` environment variable to `phuocthih21/DMX_ESP_V1`.

Example:

```bash
export GITHUB_REPO="phuocthih21/DMX_ESP_V1"
# If not already authenticated via gh, set token
export GH_TOKEN="<your_token>"

# Run the script
chmod +x scripts/create_issues_gh.sh
./scripts/create_issues_gh.sh
```

Notes:
- Each issue file contains `Labels:` and `Assignees:` header lines which the script uses.
- If `assignee` is not a valid GitHub username in the repo, GH will reject that assignment; the script will still create the issue without assignee.
- Review created issues on GitHub and adjust labels/assignees as necessary.