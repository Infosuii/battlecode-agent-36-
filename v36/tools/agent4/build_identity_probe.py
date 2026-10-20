"""Small immutable draft-only actual-SDK identity fixture (not a strength bot)."""
import argparse,hashlib,json,pathlib,shutil,subprocess,time
V36=pathlib.Path(__file__).resolve().parents[2]
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--name',default='agent4-identity-probe');parser.add_argument('--trace-returns',action='store_true');parser.add_argument('--cache',action='store_true');parser.add_argument('--cache-faults',action='store_true');a=parser.parse_args()
    name=a.name;out=V36/'builds'/name;src=out/'src';src.mkdir(parents=True,exist_ok=True)
    assert not (out/'manifest.json').exists(),'Immutable build ID already exists'
    for n in ['draft.cpp','draft.h','common.h','strategy.h','official_map_layouts.h','draft_map_cache.h']:shutil.copyfile(V36/'bot_v36/src'/n,src/n)
    shutil.copyfile(V36/'tools/agent4/identity_probe_main.cpp',src/'main.cpp')
    if a.cache_faults:
        assert a.cache
        path=src/'strategy.h';code=path.read_text();needle='        bc::draft_memory = mem;\n        return;'
        assert code.count(needle)==1,'Fixture injection must target only the cache writer'
        injection='''        // COPY-ONLY fault injection; never present in candidate strategy.h.
        const int fw = bc::map::width(), fh = bc::map::height();
        if (fw == 50 && fh == 50 && team_size == 4) mem[14] ^= 1; // rows: corrupt CRC
        if (fw == 41 && fh == 41) { // collision: missing extension
            bc::draft_memory = std::span<const std::uint8_t>{mem.data(), 6};
            return;
        }
        if (fw == 80 && fh == 80) { // expanse: unsupported schema, valid CRC
            mem[7] = 2;
            mem[14] = nova_cache::checksum(std::span<const std::uint8_t>{mem.data(), 14});
        }
        if (fw == 60 && fh == 50) { // stampede: wrong known ID, valid CRC
            mem[8] = 2;
            mem[14] = nova_cache::checksum(std::span<const std::uint8_t>{mem.data(), 14});
        }
'''
        path.write_text('#include "draft_map_cache.h"\n'+code.replace(needle,injection+needle))
    if a.trace_returns:
        path=src/'draft.cpp';code=path.read_text();start=code.index('int nova_official_map_id(');end=code.index('// Lineup per official map',start)
        section=code[start:end].replace('return cached;', 'return (std::printf("ID_RETURN line%d\\n", __LINE__), cached);')
        section=section.replace('for (const auto& row : canonical) {','for (const auto& row : canonical) { std::printf("ID_GROUP"); for (auto value : row) std::printf(" %u", value); std::printf("\\n");')
        section=section.replace('std::sort(canonical.begin(), canonical.end());',
            'std::sort(canonical.begin(), canonical.end()); for (const auto& dump : canonical) { std::printf("SDK_GROUP"); for (auto value : dump) std::printf(" %u", value); std::printf("\\n"); }')
        section=section.replace('for (auto word : row)\n                if (pos >= expected->group_words || expected->groups[pos++] != word) return (std::printf("ID_RETURN line%d\\n", __LINE__), cached);',
            'for (auto word : row)\n                if (pos >= expected->group_words || expected->groups[pos++] != word) return (std::printf("ID_WORD pos%u actual%u expected%u\\n", pos-1, word, expected->groups[pos-1]), cached);')
        path.write_text(code[:start]+section+code[end:])
    work=V36.parent;sdk=work/'development/wasi-sdk-33.0-x86_64-windows';artifact=out/(name+'.wasm')
    cmd=[str(sdk/'bin/clang++.exe'),'-O3','-DNDEBUG','-std=c++20','-msimd128','-mbulk-memory','-mbulk-memory-opt','-fwasm-exceptions','-mllvm','-wasm-use-legacy-eh=false','--no-wasm-opt','-DNOVA_STRICT_MAP_ID=1',f'-DNOVA_STRICT_MAP_CACHE={int(a.cache)}','-DAPEX_MAPINFO=1','-I'+str(work/'development/current/sdk/cpp'),'-L'+str(sdk/'share/wasi-sysroot/lib/wasm32-wasip1/eh'),'-lunwind',str(src/'main.cpp'),str(src/'draft.cpp'),'-o',str(artifact)]
    start=time.monotonic();p=subprocess.run(cmd,capture_output=True,text=True,timeout=300);(out/'build.log').write_text(p.stdout+p.stderr)
    if p.returncode:raise RuntimeError(p.stderr[-3000:])
    record=dict(id=name,artifact=str(artifact),artifact_sha256=sha(artifact),source='actual candidate draft-only SDK correctness fixture; no gameplay strength policy',
        source_files={p.name:sha(p) for p in src.iterdir()},switches=dict(NOVA_STRICT_MAP_ID=1,NOVA_STRICT_MAP_CACHE=int(a.cache),APEX_MAPINFO=1),fixture_only_fault_injection=a.cache_faults,command=cmd,seconds=time.monotonic()-start)
    (out/'manifest.json').write_text(json.dumps(record,indent=2));print(json.dumps(record))
if __name__=='__main__':main()
