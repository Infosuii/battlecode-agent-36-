"""Sequential, bounded decode of the freshly built immutable baseline corpus."""
import hashlib, json, pathlib, subprocess
V36=pathlib.Path(__file__).resolve().parents[2]
def main():
    rows=json.loads((V36/'evidence/agent4/baseline_map_replay_selection.json').read_text())
    decoder=V36.parent/'development/fast_decode.mjs'
    dest=V36/'evidence/agent4/baseline_decoded';dest.mkdir(exist_ok=True)
    manifest=[]
    for row in rows:
        path=dest/(row['map']+'.json.gz')
        if not path.exists():
            done=subprocess.run(['node',str(decoder),row['replay'],str(path)],capture_output=True,text=True,timeout=300)
            if done.returncode:raise RuntimeError(done.stderr[-2000:])
        manifest.append(dict(map=row['map'],decoded=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),replay_sha256=row['replay_sha256'],baseline_job_id=row['match_id']))
        print(json.dumps(dict(decoded=row['map'],bytes=path.stat().st_size)),flush=True)
    (dest/'manifest.json').write_text(json.dumps(dict(decoder_sha256=hashlib.sha256(decoder.read_bytes()).hexdigest(),maps=manifest),indent=2))
if __name__=='__main__':main()
