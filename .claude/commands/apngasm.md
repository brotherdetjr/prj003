Assemble an animated PNG from all `.png` files in a directory using `apngasm`.

Arguments: `$ARGUMENTS`

Parse the arguments as follows (all except the source directory are optional):
- **source directory** (required, first positional arg): directory containing input PNG frames
- **output file** (optional, second positional arg or `--output`/`-o`): path for the resulting APNG; default to `<source-dir-name>.png` next to the source directory
- **delay** (optional, `--delay`/`-d`): frame delay as a fraction `num/den` (e.g. `1/10` = 100ms); default `1/10`
- **loops** (optional, `--loops`/`-l`): number of times to loop, 0 = infinite; default `0`

Steps:
1. List all `.png` files in the source directory, sorted lexicographically. Show the list to the user.
2. If none are found, stop and report the error.
3. Construct the `apngasm` command. Convert the delay from `num/den` to `num:den` (apngasm uses colon as the fraction separator):
   ```
   apngasm -o <output> <frame1> <frame2> ... -d <num>:<den> --loops <loops> -F
   ```
4. Show the full command to the user before running it.
5. Run the command with the Bash tool.
6. Report success (output file path and frame count) or the error output.
