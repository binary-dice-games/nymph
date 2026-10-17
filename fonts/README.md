# Embedded fonts

Every `*.ttf` / `*.otf` file in this folder is compiled into the `nymph`
binary by `cmake/NymphFonts.cmake`, and every `*.txt` next to them (the
licences) too; nymph reads no font file at run time. Sources pick a font
by file name: `font: FiraSans-Regular.ttf`. `nymph fonts` lists them and
`nymph fonts --licenses` prints the licences.

| File | Family | Licence | Source |
|---|---|---|---|
| `FiraSans-Regular.ttf`, `FiraSans-Bold.ttf`, `FiraSans-Italic.ttf` | Fira Sans | SIL OFL 1.1 (`FiraSans-OFL.txt`) | [google/fonts `ofl/firasans`](https://github.com/google/fonts/tree/main/ofl/firasans) |
| `FiraMono-Regular.ttf`, `FiraMono-Bold.ttf` | Fira Mono | SIL OFL 1.1 (`FiraMono-OFL.txt`) | [google/fonts `ofl/firamono`](https://github.com/google/fonts/tree/main/ofl/firamono) |
| `PT_Serif-Web-Regular.ttf`, `PT_Serif-Web-Bold.ttf`, `PT_Serif-Web-Italic.ttf` | PT Serif | SIL OFL 1.1 (`PT_Serif-OFL.txt`) | [google/fonts `ofl/ptserif`](https://github.com/google/fonts/tree/main/ofl/ptserif) |

The files are unmodified copies from the Google Fonts repository
(fonts.google.com). To add a font, drop its file and its licence text
(`<Family>-<Licence>.txt`) here and rebuild; to embed a different set,
configure with `-DNYMPH_FONTS_DIR=<dir>`. File names must not contain `"`
or `\`. Each font adds its file size to the binary.
