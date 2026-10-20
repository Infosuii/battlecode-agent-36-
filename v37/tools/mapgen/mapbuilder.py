"""Thin Python wrapper around the engine's own MapBuilder (from the cambc map editor's WebAssembly).

Using the engine's builder guarantees the .map26a encoding and integrity checks match the real game.
Requires the `wasmtime` package.
"""
import glob
import os

import wasmtime

SYM = {'horizontal': 0, 'vertical': 1, 'rotational': 2}
CAMP = {'wolf': 0, 'elk': 1, 'gem': 2, 'gold': 3, 'monolith': 4, 'golem': 5}  # editor enum index - 1


def _engine_wasm():
    bundled = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'engine.wasm')
    hits = [p for p in [os.environ.get('BC_ENGINE_WASM', ''), bundled] if p and os.path.exists(p)]
    hits += glob.glob(os.path.expanduser('~/Library/Caches/cambc/assets/*/mapeditor/assets/v0_0_1-*.wasm'))
    hits += glob.glob(os.path.join(os.environ.get('LOCALAPPDATA', ''), 'cambc', '**', 'v0_0_1-*.wasm'), recursive=True)
    if not hits:
        raise FileNotFoundError('engine wasm not found; run `cambc mapeditor --no-browser` once to unpack assets')
    return hits[0]


class Engine:
    def __init__(self):
        cfg = wasmtime.Config()
        self.engine = wasmtime.Engine(cfg)
        self.store = wasmtime.Store(self.engine)
        wasi = wasmtime.WasiConfig()
        self.store.set_wasi(wasi)
        linker = wasmtime.Linker(self.engine)
        linker.define_wasi()
        module = wasmtime.Module.from_file(self.engine, _engine_wasm())
        self.inst = linker.instantiate(self.store, module)
        self.ex = self.inst.exports(self.store)
        self.mem = self.ex['memory']
        init = self.ex.get('_initialize') if hasattr(self.ex, 'get') else None
        try:
            self.ex['_initialize'](self.store)
        except Exception:
            pass
        self.out = self.call('Engine_Malloc', 256)

    def call(self, name, *args):
        return self.ex[name](self.store, *args)

    def check(self, code):
        if code != 0:
            raise RuntimeError(f'engine error {code}')

    def u32(self, addr):
        return int.from_bytes(self.mem.read(self.store, addr, addr + 4), 'little')

    def u64(self, addr):
        return int.from_bytes(self.mem.read(self.store, addr, addr + 8), 'little')

    def set_i32(self, addr, v):
        self.mem.write(self.store, int(v).to_bytes(4, 'little', signed=True), addr)


class MapBuilder:
    def __init__(self, eng, w=None, h=None, symmetry='rotational', file_bytes=None):
        self.e = eng
        if file_bytes is not None:
            n = len(file_bytes)
            p = eng.call('Engine_Malloc', n)
            eng.mem.write(eng.store, bytes(file_bytes), p)
            eng.check(eng.call('MapBuilder_FromFileBytes', p, n, eng.out))
            eng.call('Engine_Free', p)
        else:
            eng.check(eng.call('MapBuilder_Create', w, h, SYM[symmetry], eng.out))
        self.h = eng.u64(eng.out)

    def _c(self, name, *args):
        self.e.check(self.e.call(name, self.h, *args))

    def wall(self, x, y): self._c('MapBuilder_AddWall', x, y)
    def bush(self, x, y): self._c('MapBuilder_AddBush', x, y)
    def hole(self, x, y): self._c('MapBuilder_AddHole', x, y)
    def altar(self, x, y): self._c('MapBuilder_AddAltar', x, y)

    def camp(self, kind, x, y):
        self.e.check(self.e.call('MapBuilder_AddCamp', self.h, CAMP[kind], x, y, self.e.out))
        return self.e.u64(self.e.out)

    def team_size(self, n): self._c('MapBuilder_SetNumChampsPerTeam', n)

    def spawn(self, x0, y0, x1, y1):
        o = self.e.out
        for i, v in enumerate((x0, y0, x1, y1)):
            self.e.set_i32(o + 4 * i, v)
        self._c('MapBuilder_SetTeamASpawnArea', o)

    def get(self, what, x, y):
        self.e.check(self.e.call(f'MapBuilder_Get{what}', self.h, x, y, self.e.out))
        return self.e.mem.read(self.e.store, self.e.out, self.e.out + 1)[0]

    def verify(self): self._c('MapBuilder_VerifyIntegrity')

    def encode(self):
        o = self.e.out
        self._c('MapBuilder_Encode', o, o + 4)
        ptr, n = self.e.u32(o), self.e.u32(o + 4)
        return bytes(self.e.mem.read(self.e.store, ptr, ptr + n))

    def destroy(self):
        self.e.call('MapBuilder_Destroy', self.h)


def loads_in_game(eng, data):
    """True if the engine can create a game from these map bytes (the real runner's check)."""
    n = len(data)
    p = eng.call('Engine_Malloc', n)
    eng.mem.write(eng.store, bytes(data), p)
    code = eng.call('Game_FromMapFileBytes', p, n, eng.out, 0)
    eng.call('Engine_Free', p)
    if code != 0:
        return False
    try:
        eng.call('Game_Destroy', eng.u64(eng.out))
    except Exception:
        pass
    return True
