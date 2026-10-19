"""Compile actual scorer bodies, never a Python translation of the objective."""
from pathlib import Path
import argparse, hashlib, json, re, shutil, subprocess

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent

def method(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    # Extract by next top-level closing brace; these production methods have
    # their closing brace at column zero. No parsing/evaluation of C++ in Python.
    end = source.index('\n}', opening) + 2
    return source[start:end] + '\n'

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--compiler', default=shutil.which('g++'))
    args=p.parse_args()
    if not args.compiler: raise SystemExit('A working C++20 compiler is required')
    candidate=(ROOT/'bot_v36/src/bot.cpp').read_text(encoding='utf-8')
    baseline=(ROOT/'bot_v34/src/bot.cpp').read_text(encoding='utf-8')
    header=(ROOT/'bot_v36/src/farm_score.h').read_text(encoding='utf-8')
    learner=(ROOT/'bot_v34/src/learner.h').read_text(encoding='utf-8')
    fixture=(HERE/'scorer_fixture.cpp').read_text(encoding='utf-8')
    output=HERE.parent/'results'
    builds=output/'scorer_builds'
    builds.mkdir(parents=True,exist_ok=True)
    (output/'scorer_status.json').write_text(json.dumps({'status':'running'})+'\n')
    results={'fixture_kind':'SDK-independent production-method unit fixtures; not engine matches',
             'compiler':subprocess.check_output([args.compiler,'--version'],text=True).splitlines()[0],
             'hashes':{str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in [ROOT/'bot_v34/src/bot.cpp', ROOT/'bot_v36/src/bot.cpp', ROOT/'bot_v36/src/farm_score.h', HERE/'scorer_fixture.cpp']},
             'variants':[]}
    variants=[('disabled',False,False,False,False),
              ('corrected_production',True,False,False,False),
              ('corrected_production_diag',True,True,False,False),
              ('pruned_single',True,True,True,False),
              ('exhaustive_single',True,True,True,True),
              ('pruned_single_no_diag',True,False,True,False)]
    for learned in [False,True,'zero']:
        for name,correct,diag,single,exhaustive in variants:
            suffix='_learner_zero' if learned=='zero' else ('_learner' if learned else '')
            label=name+suffix
            build=builds/label
            build.mkdir(exist_ok=True)
            config=header
            for key,value in [('kCorrectBound',correct),('kDiagnostics',diag),('kSingleGroupDiagnostic',single),('kExhaustive',exhaustive)]:
                config=re.sub(rf'(inline constexpr bool {key} = )\w+;',rf'\g<1>{str(value).lower()};',config)
            (build/'farm_score.h').write_text(config)
            (build/'scorer_fixture.cpp').write_text(fixture)
            learner_config='#define NOVA_LEARNER 1\n#define NOVA_LEARNER_W 2.f,-3.f,1.f,4.f,-2.f,3.f,1.f,2.f,-4.f,5.f,1.f,-2.f,3.f,-1.f,2.f,4.f\n' if learned else ''
            if learned=='zero': learner_config='#define NOVA_LEARNER 1\n#define NOVA_LEARNER_W 0.f\n'
            (build/'learner_fixture.h').write_text(learner_config+learner)
            (build/'candidate_scorer.inc').write_text(method(candidate,'int Bot::score_groups('))
            legacy_include=ROOT/'bot_v36/src/farm_legacy.inc'
            if legacy_include.exists():(build/'farm_legacy.inc').write_bytes(legacy_include.read_bytes())
            (build/'baseline_scorer.inc').write_text(method(baseline,'int Bot::score_groups(').replace('Bot::score_groups(', 'Bot::score_groups_legacy(',1))
            (build/'camp_helpers.inc').write_text(''.join(method(candidate,s) for s in ['bool Bot::camp_targets_me(', 'float Bot::incoming_from_group(', 'float Bot::group_soak(']))
            prefix=candidate[candidate.index('bool Bot::farm() {'):candidate.index('    // Escort (APEX_ESCORT):')]
            prefix=prefix.replace('bool Bot::farm()', 'bool Bot::first_farm_prefix()',1)
            (build/'first_farm_prefix.inc').write_text(prefix+'    return true; // reached subsequent farming stages\n}\n')
            (build/'route_scorer.inc').write_text(method(candidate,'int Bot::route_search('))
            near=candidate[candidate.index('    struct NearCamp {'):candidate.index('    const bool doomed = NOVA_CAMP_SAFE')]
            (build/'near_safety.inc').write_text('float Bot::near_worst(const GroupRec& g, vec2 q) {\n'+near+'    return worst_at(q);\n}\n')
            exe=build/'scorer_fixture.exe'
            command=[args.compiler,'-std=c++20','-O2','-Wall','-Wextra','-Wno-unused-variable','-I',str(build),str(build/'scorer_fixture.cpp'),'-o',str(exe)]
            compiled=subprocess.run(command,text=True,capture_output=True)
            (build/'compile.log').write_text(compiled.stdout+compiled.stderr)
            if compiled.returncode: raise SystemExit(compiled.stderr)
            measured=[]
            for repeat in range(3):
                run=subprocess.run([str(exe),str(build/'choices.bin')],text=True,capture_output=True)
                (build/f'run{repeat}.log').write_text(run.stdout+run.stderr)
                if run.returncode: raise SystemExit(f'{label} failed: {run.stdout}\n{run.stderr}')
                match=re.search(r'RESULT (.*)',run.stdout)
                if not match: raise SystemExit(f'Missing RESULT in {label}')
                measured.append({k:int(v) for k,v in (part.split('=') for part in match[1].split())})
            result={'name':label,'correct_bound':correct,'diagnostics':diag,'single_group_diagnostic':single,
                    'exhaustive':exhaustive,'learner':learned,'measurements':measured,
                    'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()}
            results['variants'].append(result)
            print(label,measured[0],flush=True)
        byname={v['name']:v for v in results['variants']}
        checksum=lambda key: byname[key+suffix]['measurements'][0]['checksum']
        assert checksum('pruned_single')==checksum('exhaustive_single'), 'Pruned choice differs from exhaustive'
        assert checksum('pruned_single')==checksum('pruned_single_no_diag'), 'Diagnostics changed choices'
        assert checksum('corrected_production')==checksum('corrected_production_diag'), 'Diagnostics changed production choices'
        for left,right in [('pruned_single','exhaustive_single'),('pruned_single','pruned_single_no_diag'),('corrected_production','corrected_production_diag')]:
            assert (builds/(left+suffix)/'choices.bin').read_bytes()==(builds/(right+suffix)/'choices.bin').read_bytes(), (left,right,'decision streams differ')
        if learned=='zero':
            for name,*_ in variants:
                assert (builds/(name+suffix)/'choices.bin').read_bytes()==(builds/name/'choices.bin').read_bytes(), (name,'zero-weight identity failed')
    (output/'scorer_results.json').write_text(json.dumps(results,indent=2)+'\n')
    (output/'scorer_status.json').write_text(json.dumps({'status':'passed','variants':len(results['variants'])})+'\n')
    print('All scorer checks passed. Results:',output/'scorer_results.json')

if __name__=='__main__': main()
