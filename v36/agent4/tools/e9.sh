#!/bin/bash
set -e
export BC_ENGINE_WASM="$(cygpath -w "$LOCALAPPDATA/cambc/assets/0ba59fac5f9516e5a418a5939506eb13dbf41924dc7d64626d6edaa1d4f47157/mapeditor/assets/v1_0_1-OYnUyDob.wasm")"
for p in "p-tune 300 4001" "p-confirm 1000 4002" "p-final 400 4003"; do set -- $p
  [ -d "/c/Users/info/bcws3/$1" ] || (cd mapgen && /c/Users/info/bcvenv/Scripts/python.exe -I gen.py "C:/Users/info/bcws3/$1" $2 $3); done
for d in "APEX_TRIBUTE_CARRY=48" "APEX_TRIBUTE_CARRY=96" "APEX_END_BANK=0" "APEX_END_BANK=400" "APEX_END_BANK=800" \
         "NOVA_GOLD_UNSEEN=0.05f" "NOVA_GOLD_UNSEEN=0.20f" "NOVA_GOLD_UNSEEN=0.35f" "APEX_AGGRESSION=1.0f" "APEX_AGGRESSION=1.5f"; do
  LIMIT=100 bash tune.sh "e9_$(echo $d | tr '=.' '__')" p-tune "$d"
done
