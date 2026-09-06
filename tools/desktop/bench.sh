#!/bin/bash
# One headless baseline run. Loads a map on a local listen server, optionally adds bots,
# records 300 frames of com_speeds and a final meminfo, then quits.
#
#   tools/desktop/bench.sh <workdir> <name> <map> <cl_localPlayers bits> <vm_gameHeapMegs> <vm_cgameHeapMegs> <bots> [extra +set args]
#
# cl_localPlayers is a bitmask: 1 = one seat, 3 = two seats, 15 = four seats.
# Set DEBUG=1 to use the HUNK_DEBUG build and also emit hunklog into the home dir's console.log.
# Output: $WORK/bench/<name>.txt (and $WORK/bench/home-<name>/baseoa/console.log with DEBUG=1).
set -uo pipefail
WORK=$1; name=$2; map=$3; lp=$4; gh=$5; ch=$6; bots=$7; shift 7
BIN=$WORK/src/spearmint/build/release-linux-x86_64/spearmint_x86_64
EXTRA=""
if [ "${DEBUG:-0}" = "1" ]; then
  BIN=$WORK/src/spearmint/build/debug-linux-x86_64/spearmint_x86_64
  EXTRA="+hunklog"
fi
H=$WORK/bench/home-$name; rm -rf "$H"; mkdir -p "$H"
BOTCMDS=""
for i in $(seq 1 "$bots"); do BOTCMDS="$BOTCMDS +addbot sarge 3"; done
timeout 900 xvfb-run -a -s "-screen 0 1280x720x24" "$BIN" \
  +set fs_basepath "$WORK/run" +set fs_homepath "$H" +set fs_game baseoa \
  +set r_mode -1 +set r_customwidth 640 +set r_customheight 480 +set r_fullscreen 0 +set s_initsound 0 \
  +set com_maxfps 1000 +set logfile "${DEBUG:-0}" +set sv_pure 0 +set bot_enable 1 \
  +set vm_gameHeapMegs "$gh" +set vm_cgameHeapMegs "$ch" +set cl_localPlayers "$lp" "$@" \
  +map "$map" +wait 150 $BOTCMDS +wait 200 +exec speedson +wait 300 +exec speedsoff +meminfo $EXTRA +wait 5 +quit \
  > "$WORK/bench/$name.txt" 2>&1
echo "EXIT=$?" >> "$WORK/bench/$name.txt"
