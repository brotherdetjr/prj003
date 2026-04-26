# Session start

1. Check the current git branch (`git branch --show-current`).
2. If the branch is related to what the user is asking about, continue on it.
3. If unsure whether it's related, ask the user before proceeding.
4. If the branch is `main` or clearly unrelated to the new topic, suggest creating a new feature branch before doing any work.

# Before every commit

Before running `git commit`:

1. Review `README.md` and `PROTOCOL.md` — decide whether the changes on this branch require updates to either file. If so, make those updates and stage them before committing.
2. The `pre-commit` hook will then automatically run: clean build artifacts, format-check, unit tests, build `emu`, and Cucumber tests. A failed hook means the commit is blocked — fix the issue and retry; never use `--no-verify`.

Do not push unless the commit succeeds cleanly.
