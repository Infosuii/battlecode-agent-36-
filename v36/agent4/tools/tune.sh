#!/bin/bash
# usage: bash tune.sh EXPNAME MAPS "DEF=VAL;DEF=VAL" [base]   (engine 1.0.1 workspace)
export CAMBC_WS='C:\Users\info\bcws3' CAMBC='C:\Users\info\bcvenv3\Scripts\cambc.exe'
python paired.py --name "$1" --cand "bot_v37:$3" --base "${4:-bot_v36}" --maps "$2" --limit ${LIMIT:-0} --offset ${OFFSET:-0} | python -c "import sys,json;d=json.loads(sys.stdin.read());print(d['exp'],d['games'],d['win_rate'],d['ci95'],d['gem_diff'],d['cand_deaths'],d['base_deaths'],flush=True)"
