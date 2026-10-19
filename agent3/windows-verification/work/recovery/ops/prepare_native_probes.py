from pathlib import Path
import importlib.util,struct,json,hashlib
ROOT=Path(__file__).resolve().parents[3]
def main():
    source=Path('C:/projects/cambridge battlecode analytics/Replay Analytics/battlecode_analytics/engine.py')
    spec=importlib.util.spec_from_file_location('probe_map_engine',source);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    e=mod.Engine();e.call('MapBuilder_Create',24,24,2,e.out);h=e.u64(e.out)
    e.call('MapBuilder_SetNumChampsPerTeam',h,3);e.write(e.args,struct.pack('<4i',0,0,3,3));e.call('MapBuilder_SetTeamASpawnArea',h,e.args)
    e.call('MapBuilder_AddAltar',h,11,11)
    for x,y in [(6,2),(7,2),(8,2),(5,3)]:e.call('MapBuilder_AddCamp',h,0,x,y,e.out)
    e.call('MapBuilder_VerifyIntegrity',h);e.call('MapBuilder_Encode',h,e.out,e.out+4)
    data=e.read(e.u32(e.out),e.u32(e.out+4));path=ROOT/'work/recovery/eval/maps/camp_mechanics.map26a';path.write_bytes(data)
    code=(ROOT/'work/recovery/ops/native_camp_probe.cpp').read_text(encoding='utf8')
    for name,mode in [('probe_ren',0),('probe_varg',1),('probe_idle',2)]:
        folder=ROOT/'work/recovery/eval/bots'/name/'src';folder.mkdir(parents=True,exist_ok=True)
        (folder/'main.cpp').write_text(code.replace('probe_mode=0','probe_mode='+str(mode)),encoding='utf8')
    (ROOT/'analysis/v36/recovery/camp-probe-input.json').write_text(json.dumps({'map_sha256':hashlib.sha256(data).hexdigest(),
        'generator':'MapBuilder; native runner determines game mechanics; builder engine is not the oracle',
        'scenarios':['Ren single hit: healthy group member awake; hold in range; depart then sleep; recall outside range',
                     'Varg splash: diagonal separate group wakes; healthy distant same-group member; depart then sleep']},indent=2)+'\n')
if __name__=='__main__':main()
