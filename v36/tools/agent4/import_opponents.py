"""Register recovered artifacts under their actual names; never rebuild or relabel."""
import datetime, hashlib, json, pathlib
V36=pathlib.Path(__file__).resolve().parents[2]
WORK=V36.parent
NAMES=['spar_fish','spar_mite','spar_bush','live_platform_v35']
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def tree(folder):
    rows={p.relative_to(folder).as_posix():sha(p) for p in sorted(folder.rglob('*')) if p.is_file()}
    return dict(algorithm='sha256(sorted relative path + NUL + lowercase file sha256 + LF)',sha256=hashlib.sha256(''.join(f'{k}\0{v}\n' for k,v in rows.items()).encode()).hexdigest(),files=rows)
def main():
    seen={};out=[]
    for n in NAMES:
        artifact=(WORK/'development/binaries'/f'{n}.wasm').resolve();folder=WORK/'development/current/bots'/n;buildpath=folder/'build.json'
        build=json.loads(buildpath.read_text());digest=sha(artifact)
        assert build.get('exit_code')==0 and build.get('sha256')==digest,f'{n}: stale/failed build receipt'
        assert digest not in seen,f'Duplicate opponent artifacts {n} and {seen[digest]}'
        seen[digest]=n
        row=dict(id=n,artifact=str(artifact),artifact_sha256=digest,source='recovered historical local artifact; import only',
            imported_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_tree=tree(folder/'src'),
            source_tree_provenance='Currently available source snapshot; old build receipt lacks a source-tree digest, so exact source-to-artifact relationship is not independently proved.',
            build_receipt_path=str(buildpath.resolve()),build_receipt_sha256=sha(buildpath),build_receipt_matches_artifact=True,
            recovered_build_command=build['command'],settings=json.loads((folder/'settings.json').read_text()) if (folder/'settings.json').exists() else None,
            strength_status='Uncalibrated recovered local opponent; not the actual opponent team software and not the requested historical seven-bot pool.',
            source_description='Local behavioral approximation, not public opponent source.' if n.startswith('spar_') else 'Recovered source/archive named live_platform_v35; platform submission ID/time not established.')
        dest=V36/'builds'/n;dest.mkdir(exist_ok=True)
        target=dest/'manifest.json'
        if target.exists():
            old=json.loads(target.read_text());assert old['artifact_sha256']==digest
        else:target.write_text(json.dumps(row,indent=2))
        out.append(dict(name=n,artifact_sha256=digest,bytes=artifact.stat().st_size,build_receipt_matches=True))
    (V36/'evidence/agent4/imported_opponents.json').write_text(json.dumps(out,indent=2));print(json.dumps(out))
if __name__=='__main__':main()
