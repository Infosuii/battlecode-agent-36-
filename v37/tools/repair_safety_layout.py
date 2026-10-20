"""Correct aggregate-layout and stale-observation hazards before any match acceptance."""
from lab import ROOT,write
import shutil
def main():
    for folder in ('candidate','safe-state'):
        p=ROOT/folder/'src/world.h';s=p.read_text()
        block='#if V37_CAMP_STATE\n    int awake_state = -1;\n    int awake_round = -1;\n#endif\n'
        assert block in s;s=s.replace(block,'')
        needle='    int range = 3;     // attack range, read from the engine\'s camp design\n'
        assert needle in s;s=s.replace(needle,needle+block)
        s=s.replace('class World {\npublic:','class World {\npublic:\n#if V37_CAMP_STATE\n    int observation_round=-1;\n#endif')
        p.write_text(s,newline='\n')
        p=ROOT/folder/'src/world.cpp';s=p.read_text().replace('void World::update(int round) {','void World::update(int round) {\n#if V37_CAMP_STATE\n    observation_round=round;\n#endif')
        s=s.replace('c.awake_round>=0 && c.awake_round==groups[static_cast<std::size_t>(camp_group[ci])].seen_round','c.awake_round==observation_round')
        p.write_text(s,newline='\n')
    shutil.copytree(ROOT/'safe-state',ROOT/'safe-state2')
    write(ROOT/'evidence/safety-preflight.json',dict(rejected_build='v37-safe-state',accepted_matches=0,
        fixes=['append new CampRec fields after existing aggregate initializer fields; preserve Golem/Monolith range',
            'awake_camps_near requires actual current World observation round, never stale group seen time'],
        note='Caught in source review before running matches. Rebuild under a new immutable ID.'))
if __name__=='__main__':main()
