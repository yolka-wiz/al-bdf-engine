# Branch protection — `main`

> How the repository is guarded so nothing reaches `main` (or a release)
> without passing the gates. This describes the *intended* settings; the
> actual GitHub settings must match this. Apply via the GitHub UI
> (**Settings → Branches → Add rule**) or the REST API below.

## Rule: protect `main`

| Setting | Value |
|---|---|
| Branch | `main` |
| Require pull request reviews | ✅ — 1 approval (someone other than the author) |
| Dismiss stale approvals | ✅ |
| Require status checks | ✅ — `gate`, `asan`, `format` (hosted CI jobs from `.github/workflows/ci.yml`) |
| Require branches up to date | ✅ |
| Require conversation resolution | ✅ |
| Enforce admins | ✅ |
| Restrict who can push | Default (maintainers) — direct push effectively blocked by the PR requirement |
| Allow force pushes | ❌ |
| Allow deletions | ❌ |
| Block force pushes | ✅ |
| Do not allow bypassing | ✅ |

## Rule: protect `feature/wave1-*`-style branches (optional)

Not needed — feature branches are short-lived and their authors push to them.
If a contributor set is open to the public, consider:

| Setting | Value |
|---|---|
| Require signed commits | optional (enables commit provenance) |

## Apply via REST API

With an admin token (`GITHUB_TOKEN`):

```bash
curl -X PUT \
  -H "Authorization: Bearer $GITHUB_TOKEN" \
  -H "Accept: application/vnd.github+json" \
  https://api.github.com/repos/yolka-wiz/al-bdf-engine/branches/main/protection \
  -d '{
    "required_status_checks": {
      "strict": true,
      "contexts": ["gate", "asan", "format"]
    },
    "enforce_admins": true,
    "required_pull_request_reviews": {
      "required_approving_review_count": 1,
      "dismiss_stale_reviews": true,
      "require_code_owner_reviews": false
    },
    "restrictions": null,
    "allow_force_pushes": false,
    "allow_deletions": false,
    "required_conversation_resolution": true,
    "required_linear_history": true
  }'
```

## Notes

- The **status check names** must match the actual job IDs in
  `.github/workflows/ci.yml` (the infra-agent's W1 task defines them). If the
  workflow names jobs differently, update `contexts` accordingly.
- `required_linear_history: true` enforces rebase-merge (matches
  CONTRIBUTING.md §3b).
- Secret scanning: GitHub's **Push protection** (Settings → Code security) is
  free for public repos — enable it so even the *attempt* to push a secret is
  blocked. The repo-local pre-commit hook + hosted CI secret scan are
  defense-in-depth, not a replacement.
