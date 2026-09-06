#!/bin/bash
# Stage the SD/USB card layout for the Wii build.
#
#   tools/wii/stage_card.sh <card root> <path to your pak*.pk3 directory> [options]
#
#   <card root>   the mounted card, e.g. /e (MSYS2), /media/usb, or a staging
#                 directory you copy over afterwards
#   <pak dir>     directory holding Quake III Arena pak0..pak8.pk3 (baseq3) or
#                 OpenArena pak0..pak6-patch088.pk3 (baseoa)
#
# Options:
#   --game baseq3|baseoa   game directory (default: baseq3; use baseoa for OpenArena)
#   --debug                use the debug DOL (engine/build-wii-debug/boot.dol)
#   --patch-data DIR       existing clone of clover-moe/spearmint-patch-data
#                          (cloned into ../spearmint-patch-data if absent)
#   --gamedir NAME         data directory on the card (default: newgame; must
#                          match WII_GAMEDIR used for the DOL)
#
# Result (for --game baseq3):
#   <card>/apps/newgame/boot.dol, meta.xml
#   <card>/newgame/baseq3/pak*.pk3                 (copied)
#   <card>/newgame/baseq3/vm/mint-cgame.qvm, mint-game.qvm
#   <card>/newgame/baseq3/wii.cfg
#   <card>/newgame/baseq3/<spearmint-patch-data/baseq3 contents>
#   <card>/newgame/baseq3/<spearmint-patch-data/fallback-data contents>
#   <card>/newgame/fonts/*.ttf
#
# Run from anywhere; paths are resolved relative to the repository.
set -euo pipefail

usage() { sed -n '2,27p' "$0"; exit 1; }

[ $# -ge 2 ] || usage
CARD=$1; PAKDIR=$2; shift 2
GAME=baseq3
DEBUG=0
PATCHDATA=""
GAMEDIR=newgame
while [ $# -gt 0 ]; do
	case $1 in
		--game) GAME=$2; shift 2;;
		--debug) DEBUG=1; shift;;
		--patch-data) PATCHDATA=$2; shift 2;;
		--gamedir) GAMEDIR=$2; shift 2;;
		*) echo "unknown option $1"; usage;;
	esac
done

REPO=$(cd "$(dirname "$0")/../.." && pwd)
if [ $DEBUG = 1 ]; then
	DOL=$REPO/engine/build-wii-debug/boot.dol
else
	DOL=$REPO/engine/build-wii/boot.dol
fi
QVMDIR=$(ls -d "$REPO"/game/build/release-*/baseq3/vm 2>/dev/null | head -1 || true)

# ---- preflight ----
fail=0
[ -f "$DOL" ] || { echo "missing $DOL (run: cd engine && make -f Makefile.wii$([ $DEBUG = 1 ] && echo ' debug'))"; fail=1; }
[ -n "$QVMDIR" ] && [ -f "$QVMDIR/mint-cgame.qvm" ] && [ -f "$QVMDIR/mint-game.qvm" ] \
	|| { echo "missing QVMs (run: make -C game BUILD_GAME_SO=0 BUILD_GAME_QVM=1 BUILD_MISSIONPACK=0)"; fail=1; }
[ -d "$PAKDIR" ] && ls "$PAKDIR"/pak0.pk3 >/dev/null 2>&1 \
	|| { echo "no pak0.pk3 in $PAKDIR"; fail=1; }
[ -d "$CARD" ] || { echo "card root $CARD does not exist"; fail=1; }
[ $fail = 0 ] || exit 1

if [ -z "$PATCHDATA" ]; then
	PATCHDATA=$REPO/../spearmint-patch-data
	if [ ! -d "$PATCHDATA" ]; then
		echo "cloning spearmint-patch-data into $PATCHDATA"
		git clone -q --depth 1 https://github.com/clover-moe/spearmint-patch-data.git "$PATCHDATA"
	fi
fi
[ -d "$PATCHDATA/$GAME" ] || { echo "no $GAME directory in $PATCHDATA"; exit 1; }

DATA=$CARD/$GAMEDIR
APP=$CARD/apps/newgame

echo "staging to $CARD (game dir $GAME, data root $GAMEDIR)"
mkdir -p "$APP" "$DATA/$GAME/vm" "$DATA/fonts"

# ---- Homebrew Channel entry ----
cp "$DOL" "$APP/boot.dol"
cat > "$APP/meta.xml" <<EOF
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<app version="1">
  <name>newgame</name>
  <coder>Alex Roch</coder>
  <version>0.1 ($( [ $DEBUG = 1 ] && echo debug || echo release ))</version>
  <release_date>$(date +%Y%m%d)000000</release_date>
  <short_description>4-player splitscreen arena FPS (GameCube pads)</short_description>
  <long_description>Spearmint engine with a native GX renderer. Plug in up to four GameCube controllers; press START on an extra pad to drop in. Data lives in /$GAMEDIR on this card.</long_description>
  <ahb_access/>
</app>
EOF

# ---- game data ----
echo "copying paks from $PAKDIR"
cp -v "$PAKDIR"/*.pk3 "$DATA/$GAME/"

echo "copying QVMs from $QVMDIR"
cp -v "$QVMDIR"/mint-cgame.qvm "$QVMDIR"/mint-game.qvm "$DATA/$GAME/vm/"

echo "copying config"
cp -v "$REPO/data/wii.cfg" "$DATA/$GAME/wii.cfg"

echo "copying spearmint-patch-data ($GAME, fallback-data, fonts)"
cp -r "$PATCHDATA/$GAME/." "$DATA/$GAME/"
cp -r "$PATCHDATA/fallback-data/." "$DATA/$GAME/"
cp "$PATCHDATA"/fonts/*.ttf "$DATA/fonts/"

# ---- summary ----
echo
echo "done. Card layout:"
( cd "$CARD" && find "apps/newgame" "$GAMEDIR" -maxdepth 2 | sort | sed 's/^/  /' )
echo
echo "On the Wii: Homebrew Channel -> newgame. Pad 1 plays; press START on pads 2-4 to drop in."
echo "Debug logs (debug DOL only): $GAMEDIR/boot.txt, diag.txt, crash.txt on the card."
