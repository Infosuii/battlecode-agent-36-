"""Port inherited official-engine harness to the SDK 3.0.2 visualiser ABI."""
from lab import ROOT,WORK,write,sha
def main():
    import pathlib
    old=WORK/'battlecode-preparation/analytics/battlecode_analytics/engine.py'
    s=old.read_text().replace('SNAP = 184696','SNAP = 184760')
    (ROOT/'tools/engine_abi.py').write_text(s,newline='\n')
    old_oracle=WORK/'v36-execution/tools/engine_oracle.py'
    s=old_oracle.read_text()
    s=s.replace('from evaluate import ROOT, WORK, PROJECT, sha, write','from lab import ROOT, WORK, WS as PROJECT, sha, write')
    s=s.replace('from battlecode_analytics.engine import Engine, header, champions_full, camps_full','from engine_abi import Engine, header, champions_full, camps_full')
    s=s.replace("WORK/'development/engine-current.wasm'","ROOT/'tools/engine-current.wasm'")
    s=s.replace('184696','184760').replace("maps/rows.map26a","maps/00rows.map26a")
    (ROOT/'tools/oracle.py').write_text(s,newline='\n')
    write(ROOT/'evidence/oracle-port.json',dict(inherited_engine_sha256=sha(old),inherited_oracle_sha256=sha(old_oracle),
        current_engine_wasm_sha256=sha(ROOT/'tools/engine-current.wasm'),snapshot_bytes=184760,
        source='official PanelChrome-BXlTYAwV.js supports same table offsets and extended tail',
        scope='Ported ABI; old balance-specific assertions are historical, not automatically passing current tests'))
if __name__=='__main__':main()
