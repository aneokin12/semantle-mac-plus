# Semantle Plus

<img width="678" height="506" alt="Screenshot 2026-08-18 at 1 21 03 AM" src="https://github.com/user-attachments/assets/610c8c19-c3d2-4c18-a93d-50db41f34082" />

Semantle Plus is a small word-neighborhood game for a Macintosh Plus running
System 3 software. It uses QuickDraw and the original Event Manager, with no
floating point or runtime model loader. Type a word and press Return; the
closest indexed words are calibrated as ranks from `1/50` through `50/50`.
Words outside the indexed neighborhood retain a raw similarity score.

## Offline model pipeline

The checked-in runtime table is produced offline from the original C
word2vec implementation and the text8 corpus. The training host uses the
standard 50-dimensional model, then `tools/extract_word2vec.py`:

1. keeps the 4,096 most frequent valid lowercase words;
2. normalizes and quantizes each vector to signed bytes;
3. sorts the words alphabetically as the maintained binary-search index; and
4. writes `src/model_data.h` for the classic build.

Run the full training loop when a new corpus or model is wanted:

```sh
./tools/train_word2vec.sh
```

It caches the upstream trainer, text8, and its binary model in
`.word2vec-work`. Set `FORCE_WORD2VEC_TRAIN=1` to retrain. The vocabulary,
dimension, and thread count can be changed with `WORD2VEC_VOCAB_SIZE`,
`WORD2VEC_DIMENSION`, and `WORD2VEC_THREADS`; the runtime constants and
extractor arguments must remain in agreement.

The 4,096 × 50 signed-byte vector table is about 205 KB. The fixed 24-byte
word index is about 98 KB, and the resulting MacBinary application is about
314 KB, leaving substantial room under a 1 MB application budget. A round
does one 4,096-word integer scan to prepare its top-50 list; each guess then
uses binary lookup and one 50-component integer dot product.

Indexed guesses in the target's top 50 display `x/50`, with status messages
that identify the strength of the neighborhood hit. Unknown or malformed
words use a deterministic character n-gram fallback and are explicitly
reported as not indexed; empty submissions are rejected.

## Build and test

The portable similarity engine can be checked on the development Mac:

```sh
make test
```

The classic application is built with the Retro68 m68k-apple-macos toolchain:

```sh
nix develop github:autc04/Retro68#m68k
export RETRO68_TOOLCHAIN=/path/to/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
./build.sh
```

If using a Retro68 install that exposes `RETRO68_PREFIX`, set that variable
instead. The build directory contains a classic APPL/MacBinary output and a
raw HFS `.dsk` image; the latter can be used with Mini vMac or copied to a
real Macintosh with a resource-fork-preserving transport.

## CD-ROM image

After a successful Retro68 build on macOS, run:

```sh
nix develop github:autc04/Retro68#m68k --command ./package-cdrom.sh
```

The script creates `dist/SemantlePlus.cdr`, an Apple Partition Map whose HFS
partition is populated by Retro68's HFS tools. This keeps the volume classic
HFS even on current macOS, where `hdiutil -fs HFS` falls back to HFS+. Burn the
image at a conservative speed with an HFS-aware CD mastering tool.

The Macintosh Plus needs a compatible external SCSI CD-ROM drive/driver;
System 3 does not provide every later CD-ROM driver automatically.

## Downloads

Prebuilt distributables are kept in [`artifacts/`](artifacts/):

- [`SemantlePlus.cdr`](artifacts/SemantlePlus.cdr) — 20 MB classic-HFS CD image
  for burning or mounting with a compatible Macintosh setup.
- [`SemantlePlus.dsk`](artifacts/SemantlePlus.dsk) — 800 KB HFS disk image for
  Mini vMac and similar emulators.
- [`SemantlePlus.bin`](artifacts/SemantlePlus.bin) — 314 KB MacBinary
  application with its resource fork preserved.

## Controls

- Type letters, Backspace/Delete to edit, Return to score.
- Click NEW ROUND for another random target.
- Choose File > Quit or press Command-Q.

The UI avoids later APIs such as `WaitNextEvent`; the event loop uses
`GetNextEvent` and `SystemTask`, keeping the application appropriate for the
System 3 target.
