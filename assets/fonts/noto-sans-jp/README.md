# Playback font

Noto Sans JP Regular 2.004 supplies the Japanese baseline and shared glyphs.
Pinned Noto Sans CJK SC/KR Regular 2.004 fonts supply missing Chinese and Korean
characters. All fonts are copyright 2014–2021 Adobe, SIL OFL 1.1 (see [LICENSE](LICENSE)).
[Japanese source](https://github.com/notofonts/noto-cjk/blob/Sans2.004/Sans/SubsetOTF/JP/NotoSansJP-Regular.otf)
is retained unmodified. [Fallback source pins](fallback-sources.json) record
upstream URLs and SHA-256 hashes; the two large fallback OTFs are host-only inputs,
not firmware files.

The default 16 px / 4-bit profile contains **10,702 unique glyphs, 1,477,697 bytes**:

- Common Japanese: CP932-mappable characters, Latin-1, punctuation, kana and
  fullwidth/halfwidth forms mapped by the Japanese source.
- [1,500 Chinese Han](chinese-top1500.txt), selected by frequency in Jun Da's
  modern-Chinese corpus (2004-03-30), restricted to GB2312 before taking 1,500.
  This is frequency order, not the first 1,500 GB2312 codepoints.
- 2,350 common KS X 1001 Hangul syllables, modern leading/vowel/trailing Jamo and
  compatibility Jamo. No Korean Hanja rows are added.
- [32 anonymous library supplements](extra-codepoints.txt) supported by the
  Japanese source outside the baseline.

Every Unicode scalar is stored once. Japanese glyphs take priority, including
1,046 Chinese characters already present in the Japanese baseline/supplements.
Distinct simplified and traditional scalars remain distinct. Source counts are
7,908 JP, 376 SC and 2,418 KR. Korean text is composed algorithmically (L+V+T or
LV+T) during measurement, fitting and drawing. This is not general NFC or shaping
support. Unsupported glyphs use `?`, including emoji and less common Hangul.

The 2026-11-16 complete Tab5 audit contained 20,916 songs, 2,130 albums,
1,090 artists, two playlists and 14 queue entries. After Hangul composition,
2,453 of 2,555 distinct metadata scalars are covered. The remaining 102 include
rare ideographs, other scripts, emoji, styled letters and control/invisible
characters. Raw library metadata is kept only in ignored local backups.

Regeneration (Pillow 12.0.0, fontTools 4.60.1, CairoSVG 2.8.2):

```sh
.venv/bin/python scripts/generate_playback_font.py --fetch-sources
# Later regenerations use the cached, hash-checked sources offline:
.venv/bin/python scripts/generate_playback_font.py
.venv/bin/python scripts/audit_playback_font.py --library backups/font-subset/library.json --report backups/font-subset/coverage.json
```

`--source-dir` selects the fallback OTF cache (default `backups/font-subset/sources`).
`--full` regenerates the historical full Japanese-only profile, not full CJK.
Normal firmware builds use checked-in flash data and need no font tooling or
network. Sorted CPF1 metrics/bitmaps stay in flash, with no whole-font RAM copy.
