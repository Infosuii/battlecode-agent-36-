#!/bin/bash
set -e
export BC_ENGINE_WASM="$(cygpath -w "$LOCALAPPDATA/cambc/assets/9f2cf433a04e5a113a5488a236703e1473411d687bb65322214eec98b96fa13a/mapeditor/assets/v0_0_3-zaIi77qe.wasm")"
[ -d /c/Users/info/bcws/maps-final ] || (cd mapgen && /c/Users/info/bcvenv/Scripts/python.exe -I gen.py "C:/Users/info/bcws/maps-final" 600 3701)
s() { python -c "import sys,json;d=json.loads(sys.stdin.read());print(d['exp'],d['games'],d['win_rate'],d['ci95'],d['gem_diff'],d['cand_deaths'],d['base_deaths'],d['same_draft_games'],flush=True)"; }
python paired.py --name e7_parity_off --cand "bot_v36:NOVA_V36_DRAFT=0" --base bot_v34 --maps maps-gen | s
python paired.py --name e7_official --cand bot_v36 --base bot_v34 --maps maps | s
python paired.py --name e7_final --cand bot_v36 --base bot_v34 --maps maps-holdout,maps-final | s
