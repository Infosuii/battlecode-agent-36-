"""Safely read fixed members from CLI replay ZIPs or raw engine replays.

No archive paths are extracted. Each accepted member is read by an exact name.
Fuel and stdout are diagnostics, not a release-policy dataset by themselves.
"""
import argparse, hashlib, json, pathlib, zipfile

MEMBERS = ('replay.repl26a','team-a.stdout','team-b.stdout','team-a.debug','team-b.debug','team-a.fuel','team-b.fuel')

def read_bundle(path):
    p=pathlib.Path(path)
    if zipfile.is_zipfile(p):
        with zipfile.ZipFile(p) as z:
            if len(z.infolist())>64: raise ValueError('Unexpected replay bundle member count')
            result={}
            for name in MEMBERS:
                matches=[i for i in z.infolist() if i.filename==name]
                if len(matches)>1: raise ValueError('Duplicate archive member '+name)
                if matches:
                    if matches[0].file_size>256*1024*1024: raise ValueError('Oversize member '+name)
                    result[name]=z.read(name)
            if 'replay.repl26a' not in result: raise ValueError('Replay ZIP missing raw replay')
            return result
    raw=p.read_bytes()
    if not raw.startswith(b'a62per'): raise ValueError('Unrecognized raw replay header')
    return {'replay.repl26a':raw}

def telemetry(member):
    return [line for line in member.decode('utf-8',errors='replace').splitlines()
            if any(tag in line for tag in ('RB ','RBSKIP ','TDV ','TSDROP ','NOVA_MAPINFO '))]

def main():
    parser=argparse.ArgumentParser();parser.add_argument('bundle');parser.add_argument('--outdir');args=parser.parse_args()
    members=read_bundle(args.bundle)
    if args.outdir:
        out=pathlib.Path(args.outdir);out.mkdir(parents=True,exist_ok=True)
        for name,data in members.items(): (out/name).write_bytes(data)
    print(json.dumps({'path':args.bundle,'members':{n:{'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()} for n,b in members.items()},
          'telemetry':{n:telemetry(b) for n,b in members.items() if n.endswith('.stdout')}}))

if __name__=='__main__':main()
