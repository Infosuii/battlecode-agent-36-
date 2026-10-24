"""Keep the disabled scorer's actual baseline token stream in a compiled branch."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
def main():
    original=(ROOT/'bot_v34/src/bot.cpp').read_text(encoding='utf-8')
    path=ROOT/'bot_v36/src/bot.cpp'
    candidate=path.read_text(encoding='utf-8')
    if '#include "farm_legacy.inc"' in candidate:return
    signature='int Bot::score_groups(int participants_hint, int& best_need, float& best_score) {'
    start=original.index(signature)+len(signature)
    end=original.index('\n}',start)
    legacy=original[start:end]
    (ROOT/'bot_v36/src/farm_legacy.inc').write_bytes(('// Exact frozen scoring body for all-new-switches-off code generation.\n'+legacy+'\n').encode())
    start=candidate.index(signature)+len(signature)
    end=candidate.index('\n}',start)
    body=candidate[start:end]
    replacement='\n    if constexpr (!(farm_score::kCorrectBound || farm_score::kDiagnostics ||\n        farm_score::kSingleGroupDiagnostic || farm_score::kExhaustive)) {\n#include "farm_legacy.inc"\n    } else {'+body+'\n    }'
    result=candidate[:start]+replacement+candidate[end:]
    path.write_bytes(result.replace('\n','\r\n').encode())
    print('Preserved exact legacy scorer in disabled compile-time branch')
if __name__=='__main__':main()
