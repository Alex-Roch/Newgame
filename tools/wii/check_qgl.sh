#!/bin/bash
# Every qgl* entry point the renderergl1 frontend calls must be assigned in
# QGL_InitGX (code/renderergx/qgl_gx.c); an unassigned one is a NULL call on
# the console. Run from the repository root.
E=engine/code
used=$(grep -ohE "\bqgl[A-Za-z0-9_]+" $E/renderergl1/*.c $E/renderercommon/tr_font.c $E/renderercommon/*.c | sort -u)
missing=0
for q in $used; do
  case $q in qglMajorVersion|qglMinorVersion|qglesMajorVersion|qglesMinorVersion) continue;; esac
  if ! grep -qE "^\s*$q\s*=" $E/renderergx/qgl_gx.c; then echo "NOT WIRED: $q"; missing=1; fi
done
[ $missing = 0 ] && echo "check_qgl: all $(echo "$used" | wc -l) qgl entry points are wired"
exit $missing
