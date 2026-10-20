#!/bin/bash
# E5 screen; run from v36/agent4/tools
set -e
export BC_ENGINE_WASM="$(cygpath -w "$LOCALAPPDATA/cambc/assets/9f2cf433a04e5a113a5488a236703e1473411d687bb65322214eec98b96fa13a/mapeditor/assets/v0_0_3-zaIi77qe.wasm")"
for s in 3 4 5; do
  [ -d "/c/Users/info/bcws/maps-t$s" ] || (cd mapgen && GEN_TEAM=$s /c/Users/info/bcvenv/Scripts/python.exe -I gen.py "C:\Users\info\bcws\maps-t$s" 300 361$((s-1)))
done
REST="cd::prism,cd::ryth,cd::vendetta,cd::willow,cd::marcella,cd::vesper_vale,cd::tavi,cd::ren,cd::varg,cd::stellar,cd::cassi_and_issac"
L2="cd::cassi_and_issac,cd::stellar,cd::varg,cd::tavi,cd::ren,cd::prism,cd::ryth,cd::vendetta,cd::willow,cd::marcella,cd::vesper_vale"
L19="cd::cassi_and_issac,cd::stellar,cd::ren,cd::ryth,cd::varg,cd::tavi,cd::prism,cd::willow,cd::vendetta,cd::marcella,cd::vesper_vale"
L6="cd::vendetta,cd::varg,cd::cassi_and_issac,cd::ryth,cd::ren,cd::marcella,cd::stellar,cd::tavi,cd::prism,cd::willow,cd::vesper_vale"
run() { python paired.py --name "$1" --cand "bot_v36:NOVA_UNSEEN3=$2;NOVA_UNSEEN4=$3;NOVA_UNSEEN5=$4" --base "$5" --maps "$6" | python -c "import sys,json;d=json.loads(sys.stdin.read());print(d['exp'],d['games'],d['win_rate'],d['ci95'],d['gem_diff'],d['cand_deaths'],d['base_deaths'],flush=True)"; }
run e5_s3_srr "cd::stellar,cd::ryth,cd::ren,$REST" $L2 $L2 bot_v34 maps-t3
run e5_s4_crrv "cd::cassi_and_issac,cd::ren,cd::ryth,cd::varg,$REST" $L2 $L2 bot_v34 maps-t4
run e5_s4_l6 $L2 $L6 $L2 bot_v34 maps-t4
run e5_s5_crstw "$L2" $L2 "cd::cassi_and_issac,cd::ren,cd::stellar,cd::tavi,cd::willow,$REST" bot_v34 maps-t5
run e5_s5_crrst "$L2" $L2 "cd::cassi_and_issac,cd::ren,cd::ryth,cd::stellar,cd::tavi,$REST" bot_v34 maps-t5
run e5_s5_l19 $L2 $L2 $L19 bot_v34 maps-t5
