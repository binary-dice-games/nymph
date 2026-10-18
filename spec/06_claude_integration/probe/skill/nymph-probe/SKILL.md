---
name: nymph-probe
description: Test skill for nymph (spec/06_claude_integration step 1). Use when the user asks to run the nymph probe or to draw a test chart with nymph.
---

# nymph probe

This skill checks that the bundled `nymph` executable runs in this
sandbox and that a PNG it writes reaches the user.

1. Run `uname -m`, then `chmod +x bin/nymph && bin/nymph --version` from
   this skill's folder. Report both outputs.
2. Write this file as `chart.yaml` in the working directory:

   ```yaml
   root:
     type: Plot
     title: nymph probe
     children:
       - {type: PlotLine, label: values, ys: [3, 1, 4, 1, 5, 9, 2, 6]}
   ```

3. Run `bin/nymph render chart.yaml -o chart.png --rasterizer software`
   (use the skill folder's path to `bin/nymph`) and report the exit code.
4. Look at `chart.png` and describe it in one sentence.
5. Share `chart.png` with the user as an output file.
6. Tell the user, step by step, which commands succeeded.
