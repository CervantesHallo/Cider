# R v1 source reconstruction

`cider-r-v1-source-20261102.json` is a **source-only plan**, not an executable
engine recipe. `engine/build.sh` rejects it before build side effects.

Use the existing exact source archive:

```sh
python3 engine/prepare-r-v1.py \
  --archive /path/to/wine-11.18-7b3fff76-source.tar.gz \
  --output out/a-new-r-v1-source-directory
```

The preparer checks the source archive and each patch's SHA256, requires a new
output directory and the expected peak plus 20 GiB free, and applies the selected
patches with zero fuzz/offset. It then compares the recorded qualified component
sources. It downloads nothing, compiles nothing and never executes Wine.

The synchronization chain selects 0100–0104, 0113, 0115, 0117 and 0120–0123;
alternative experimental branches and diagnostic/compiler profiles are excluded.
The ordinary port queue is applied through0032. The latter registers shared
headers for the clean-tree dependency generator; it changes no runtime code.

Whole-source compilation, portable PE C++/CRT and modified DXMT inputs, all
dependency/addon hashes and complete engine packaging remain separate work.
New native binaries also require their own actual child-policy qualification;
an existing manifest or source verdict cannot admit different bytes.
