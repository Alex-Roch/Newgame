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
#   --image FILE           write into a raw FAT32 SD image (Dolphin's WiiSD.raw)
#                          with mtools instead of a directory; <card root> is
#                          then a staging directory that is created if needed.
#                          A missing image is created with --image-size.
#   --image-size SIZE      size for a newly created image (default 2G)
#   --dolphin              stage into Dolphin's SD sync folder
#                          (Load/WiiSDSync under the Dolphin user directory);
#                          <card root> is ignored. Needs no mtools: enable
#                          Config > Wii > "Automatically Sync with Folder".
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

usage() { sed -n '2,28p' "$0"; exit 1; }

[ $# -ge 2 ] || usage
CARD=$1; PAKDIR=$2; shift 2
GAME=baseq3
DEBUG=0
PATCHDATA=""
GAMEDIR=newgame
IMAGE=""
IMAGESIZE=2G
DOLPHIN=0
while [ $# -gt 0 ]; do
	case $1 in
		--game) GAME=$2; shift 2;;
		--debug) DEBUG=1; shift;;
		--patch-data) PATCHDATA=$2; shift 2;;
		--gamedir) GAMEDIR=$2; shift 2;;
		--image) IMAGE=$2; shift 2;;
		--image-size) IMAGESIZE=$2; shift 2;;
		--dolphin) DOLPHIN=1; shift;;
		*) echo "unknown option $1"; usage;;
	esac
done

# Dolphin: "Config > Wii > SD Card > Automatically Sync with Folder" mirrors
# this directory into WiiSD.raw on every boot (and back on shutdown).
if [ $DOLPHIN = 1 ]; then
	if [ -n "${APPDATA:-}" ]; then
		CARD=$(cygpath -u "$APPDATA" 2>/dev/null || echo "$APPDATA")/"Dolphin Emulator/Load/WiiSDSync"
	elif [ -d "$HOME/Library/Application Support/Dolphin" ]; then
		CARD="$HOME/Library/Application Support/Dolphin/Load/WiiSDSync"
	else
		CARD="$HOME/.local/share/dolphin-emu/Load/WiiSDSync"
	fi
	mkdir -p "$CARD"
fi
if [ -n "$IMAGE" ]; then
	# MSYS2 ships mtools only as a MinGW package (/mingw64/bin), which the
	# plain MSYS and devkitPro shells do not have on PATH.
	if ! command -v mcopy >/dev/null && [ -x /mingw64/bin/mcopy.exe ]; then
		PATH=/mingw64/bin:$PATH
	fi
	command -v mcopy >/dev/null || {
		echo "mtools (mcopy/mformat) is required for --image."
		echo "  MSYS2:  pacman -S mingw-w64-x86_64-mtools"
		echo "  Debian: apt install mtools      macOS: brew install mtools"
		echo "Or skip mtools entirely: --dolphin stages into Dolphin's sync folder."
		exit 1
	}
	mkdir -p "$CARD"
fi

# Native (MinGW) mtools wants Windows paths; cygpath -m gives C:/... form.
hostpath() { if command -v cygpath >/dev/null; then cygpath -m "$1"; else printf '%s' "$1"; fi; }

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
	|| { echo "no pak0.pk3 in $PAKDIR"
	     echo "  point <pak dir> at your Quake III Arena baseq3 directory, e.g."
	     echo "    /c/Program Files (x86)/Steam/steamapps/common/Quake 3 Arena/baseq3"
	     echo "    /c/GOG Games/Quake III Arena/baseq3"
	     echo "    /c/Program Files (x86)/Quake III Arena/baseq3   (CD install)"
	     echo "  or your OpenArena baseoa directory with --game baseoa"; fail=1; }
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

if [ -n "$IMAGE" ]; then
	if [ ! -f "$IMAGE" ]; then
		echo "creating $IMAGESIZE FAT32 image $IMAGE"
		truncate -s "$IMAGESIZE" "$IMAGE"
		mformat -i "$(hostpath "$IMAGE")" -F ::
	else
		# Dolphin creates WiiSD.raw at 128 MB by default; the Q3 paks alone
		# are about 470 MB. Refuse rather than let mcopy fail half way.
		need=$(du -sb "$CARD/apps" "$CARD/$GAMEDIR" | awk '{s+=$1} END {print s}')
		have=$(stat -c %s "$IMAGE")
		if [ "$have" -lt $((need + 16 * 1024 * 1024)) ]; then
			echo "$IMAGE is $((have / 1048576)) MB but the card contents need about $((need / 1048576)) MB."
			echo "Delete the image (this script recreates it at --image-size) or pass a new path,"
			echo "and set Dolphin's Config > Wii > 'SD Card File Size' to match or to Auto."
			exit 1
		fi
	fi
	echo "writing card contents into $IMAGE"
	# mtools: -s recurses, -o overwrites, -m preserves times; ::/ is the image root
	mcopy -i "$(hostpath "$IMAGE")" -s -o -m "$(hostpath "$CARD/apps")" "$(hostpath "$CARD/$GAMEDIR")" ::/
	mdir -i "$(hostpath "$IMAGE")" ::/
	echo
	echo "Dolphin: Config > Wii > SD Card, tick 'Insert SD Card', and point 'SD Card Path' at $IMAGE"
	echo "(or copy it over Load/WiiSD.raw). Boot engine/build-wii/boot.dol with File > Open."
fi

echo
echo "On the Wii: Homebrew Channel -> newgame. Pad 1 plays; press START on pads 2-4 to drop in."
echo "Debug logs (debug DOL only): $GAMEDIR/boot.txt, diag.txt, crash.txt on the card."
