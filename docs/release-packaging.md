# NetSurf MUI release packaging

Use `tools/package_release.sh` whenever you finish a commit and want to
publish an AmigaOS build as an LhA archive directly via git (no GitHub CLI
required).

```bash
# from repo root, after building NetSurf
./tools/package_release.sh            # auto dates archive + tag
./tools/package_release.sh 2025-11-15 # optional explicit stamp
```

What the script does:

1. Confirms the `NetSurf` binary exists and `lha` is installed.
2. Creates `NetSurf-<timestamp>.lha` using Amiga-native compression.
3. Spins up a temporary worktree that tracks (or creates) the `release-packages`
  branch (override with `PUBLISH_BRANCH`).
4. Copies the archive into that worktree, commits it (`Release archive …`), and
  pushes the branch via plain `git push origin <branch>`.
5. Tears down the temporary worktree and deletes the `.lha` from your working
  tree so you stay clean.

Environment overrides:

- `PUBLISH_BRANCH` – change the branch that collects LhA artifacts
  (`release-packages` by default).
- Pass a manual timestamp as the first argument if you prefer predictable
  filenames (e.g. CI builds).

The `release-packages` branch becomes the canonical place to fetch packaged
builds (each commit contains exactly one archive). Consumers can browse it on
GitHub, clone it directly, or cherry-pick specific blobs without touching the
main development history.

> Tip: If you want this to run automatically after each commit, symlink the
> script into `.git/hooks/post-commit` or invoke it from whatever automation
> you use to record commits. The release branch will keep growing with dated
> archives, while `main` stays lean.
