# Session start

1. Check the current git branch (`git branch --show-current`).
2. If the branch is related to what the user is asking about, continue on it.
3. If unsure whether it's related, ask the user before proceeding.
4. If the branch is `main` or clearly unrelated to the new topic, suggest creating a new feature branch before doing any work.
5. If the hook reports **MERGED UNDELETED BRANCHES**, list them, offer to delete them, and — if the user approves — delete each one locally (`git branch -d`) and on origin (`git push origin --delete`).

# Editing files

When the purpose of a change is not text reformatting, do not introduce incidental whitespace or formatting changes (extra blank lines, reindentation, etc.).

# Before every commit

Before running `git commit`:

1. Review `README.md` and `PROTOCOL.md` — decide whether the changes on this branch require updates to either file. If so, make those updates and stage them before committing.
2. The `pre-commit` hook will then automatically run: clean build artifacts, format-check, unit tests, build `emu`, and Cucumber tests. A failed hook means the commit is blocked — fix the issue and retry; never use `--no-verify`.

# After every commit

Always push immediately after a successful commit — even if the user only said "commit". If the remote is `github.com` and this is the first push of the branch (no upstream set yet):

1. Check whether `gh` is available (`gh --version`).
2. If not, offer to help install it (see README Setup section). If the user declines, skip PR creation and save a memory note so you never ask again.
3. If `gh` is available (or the user installs it), create a PR with `gh pr create`.
