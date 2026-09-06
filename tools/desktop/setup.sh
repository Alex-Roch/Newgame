#!/bin/bash
# Reproducible desktop baseline: clone, build, and stage data for Spearmint + mint-arena.
#
#   tools/desktop/setup.sh [workdir]
#
# Produces:
#   $WORK/src/{spearmint,mint-arena,spearmint-patch-data}
#   $WORK/src/spearmint/build/release-linux-x86_64/spearmint_x86_64
#   $WORK/src/spearmint/build/debug-linux-x86_64/spearmint_x86_64   (HUNK_DEBUG, for hunklog)
#   $WORK/run/baseoa/{pak*.pk3 (symlinks), vm/mint-{cgame,game}.qvm, patch data, fallback data}
#   $WORK/run/fonts/*.ttf
#
# Test content: OpenArena 0.8.8 (GPL) is downloaded from archive.org unless OA_ZIP points at a
# local copy. Quake III Arena data can be used instead by staging a baseq3/ directory yourself.
#
# Host packages (Ubuntu 24.04): build-essential libsdl2-dev libgl1-mesa-dev xvfb mesa-utils
# python3 unzip curl git.
set -euo pipefail
WORK=${1:-$PWD/desktop-baseline}
SPEARMINT_REV=${SPEARMINT_REV:-c48bdf60e66d25b391f2be02a5b91c98b3464ce1}
MINTARENA_REV=${MINTARENA_REV:-482d52830e074996bb818ec4cf149a902c57f925}
JOBS=${JOBS:-$(nproc)}
mkdir -p "$WORK/src" "$WORK/run/baseoa/vm" "$WORK/run/fonts" "$WORK/data"
cd "$WORK/src"
[ -d spearmint ] || git clone -q https://github.com/clover-moe/spearmint.git
[ -d mint-arena ] || git clone -q https://github.com/clover-moe/mint-arena.git
[ -d spearmint-patch-data ] || git clone -q https://github.com/clover-moe/spearmint-patch-data.git
git -C spearmint checkout -q "$SPEARMINT_REV"
git -C mint-arena checkout -q "$MINTARENA_REV"

# Engine: gl1 only, renderer linked statically, no autoupdater. Everything else at Spearmint defaults
# (internal freetype/ogg/vorbis/opus/mp3/jpeg/zlib, curl and OpenAL via dlopen).
COMMON="BUILD_SERVER=0 BUILD_RENDERER_OPENGL2=0 USE_RENDERER_DLOPEN=0 USE_AUTOUPDATER=0"
make -C spearmint -j"$JOBS" $COMMON
make -C spearmint -j"$JOBS" $COMMON debug

# Game code as QVMs (and native .so for symbol-size inspection).
make -C mint-arena -j"$JOBS" BUILD_GAME_SO=0 BUILD_GAME_QVM=1 BUILD_MISSIONPACK=0
make -C mint-arena -j"$JOBS" BUILD_GAME_SO=1 BUILD_GAME_QVM=0 BUILD_MISSIONPACK=0

# Test content.
if [ -z "${OA_ZIP:-}" ]; then
  OA_ZIP="$WORK/data/openarena-0.8.8.zip"
  [ -f "$OA_ZIP" ] || curl -sS -L -o "$OA_ZIP" https://archive.org/download/openarena-0.8.8/openarena-0.8.8.zip
fi
[ -d "$WORK/data/oa/openarena-0.8.8/baseoa" ] || unzip -q -o "$OA_ZIP" -d "$WORK/data/oa"
ln -sf "$WORK"/data/oa/openarena-0.8.8/baseoa/*.pk3 "$WORK/run/baseoa/"
cp -r spearmint-patch-data/baseoa/* "$WORK/run/baseoa/"
cp -r spearmint-patch-data/fallback-data/* "$WORK/run/baseoa/"
cp spearmint-patch-data/fonts/*.ttf "$WORK/run/fonts/"
cp mint-arena/build/release-linux-x86_64/baseq3/vm/*.qvm "$WORK/run/baseoa/vm/"
echo "com_speeds 1" > "$WORK/run/baseoa/speedson.cfg"
echo "com_speeds 0" > "$WORK/run/baseoa/speedsoff.cfg"
echo "Ready. Run: tools/desktop/bench.sh $WORK <name> <map> <cl_localPlayers bits> <vm_gameHeapMegs> <vm_cgameHeapMegs> <bots>"
