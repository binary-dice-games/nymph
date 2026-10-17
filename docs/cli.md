# nymph — Command Line

```sh
nymph chart.yaml              # render: writes chart.png
nymph chart.png               # extract: prints the YAML the image carries
nymph check chart.yaml        # validate only; prints nothing when valid
nymph schema PlotLine         # what PlotLine accepts
```

`nymph` needs no display, window or GPU: it draws on the CPU.

## Commands

| Command | What it does |
|---|---|
| `nymph <in> [-o <out>]` | If `<in>` is a PNG, prints its YAML (or writes it to `<out>`). Otherwise renders `<in>` to `<out>`, by default `<in>` with its extension replaced by `.png`. |
| `nymph render <in> [-o <out>]` | Renders. `<in>` may be a nymph PNG (its YAML is rendered again, in place unless `-o`) or `-` for standard input, which needs `-o`. |
| `nymph extract <in.png> [-o <out>]` | Prints the YAML a nymph PNG carries, or writes it to `<out>`. |
| `nymph check <in>` | Validates a YAML file (or a nymph PNG's YAML): everything `render` checks, including whether a layout fits, without drawing. |
| `nymph schema` | Prints every element type with its fields, types, defaults and allowed values. |
| `nymph schema <Type>` | The same for one type. |
| `nymph schema --markdown` | The reference as Markdown ([elements.md](elements.md) is generated from it). |
| `nymph fonts` | The font names `font:` accepts, one per line: `default` and every embedded font file. |
| `nymph fonts --licenses` | The licence texts of the embedded fonts. |
| `nymph --version`, `nymph --help` | Version, usage. |

`-o -` writes to standard output, so nymph works in a pipe:

```sh
generate-yaml | nymph render - -o - > chart.png
```

## Exit codes and errors

| Code | Meaning |
|---|---|
| 0 | Success. |
| 1 | Failure; one line on standard error. |
| 2 | Bad arguments; the message and the usage on standard error. |

A problem in the source names its line and column (columns count
characters):

```
nymph: chart.yaml:9:56: unknown column 'revenu' (did you mean 'revenue'?) (columns: month, revenue, cost)
```

Anything else names the file:

```
nymph: photo.png: not a nymph image (the PNG carries no nymph source)
nymph: missing.yaml: no such file
```

On failure nothing is written: output goes to a temporary file that is
renamed over the target only once it is complete, so an existing output
file is left as it was. A symbolic link as `-o` is followed, and a
device or pipe (`-o /dev/null`) is written directly.

## Metadata survives only exact copies

The YAML is stored in the PNG as a text chunk. Tools that re-encode images
(many chat apps and image hosts) drop it; keep the YAML file too when the
image goes through one.
