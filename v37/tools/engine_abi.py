"""Replay decoder for Cambridge Battlecode 2026 (.repl26a) using the engine's own wasm.
Layout reverse-engineered from the visualiser JS (PanelChrome/index chunks), snapshot ABI 1 (184696 bytes)."""
import struct
from pathlib import Path
import wasmtime

WASM = Path(__file__).resolve().parents[1] / "engine" / "engine.wasm"

STEP_KINDS = ("Phase.Respawn.Capture.Tick.Move.Attack.Cast ability.Input.End turn.Set priority..Recall."
              "Submit gems.Shop.Place ward.Consumable.Cancel channel.Sandbox.Gems submitted.Draft champion."
              "Place altar lock.Teleport channel.Teleport complete.Movement blocked.Resign.Sell item.Turn rollover").split(".")
PHASES = {0: "Initial", 1: "CampRespawns", 4: "Spells", 7: "CampCaptures", 9: "Camps", 10: "Champions",
          11: "Finished", 12: "Draft", 13: "ChampionEndTurn", 14: "ChampionStartTurn"}
CHAMP = ["Ryth", "Vesper", "Vendetta", "Cassi", "Issac", "Stellar", "Prism", "Marcella", "Willow", "Ren", "Varg",
         "Tavi", "HeartOfRuin"]
DRAFT_CHAMP = ["Ryth", "Vesper", "Vendetta", "C&I", "Stellar", "Prism", "Marcella", "Willow", "Ren", "Varg", "Tavi",
               "HeartOfRuin"]
ITEMS = ("Monster Shard of Strength.Monster Emblem of Strength.Monster Shard of Focus.Monster Emblem of Focus."
         "Monster Shard of Health.Monster Emblem of Health.Power Shard of Strength.Power Emblem of Strength."
         "Power Shard of Focus.Power Emblem of Focus.Power Shard of Health.Power Emblem of Health."
         "Durability Shard of Strength.Durability Emblem of Strength.Durability Shard of Focus."
         "Durability Emblem of Focus.Durability Shard of Health.Durability Emblem of Health."
         "Treasure Shard of Strength.Treasure Emblem of Strength.Treasure Shard of Focus.Treasure Emblem of Focus."
         "Treasure Shard of Health.Treasure Emblem of Health.Jungle Shard of Strength.Jungle Emblem of Strength."
         "Jungle Shard of Focus.Jungle Emblem of Focus.Jungle Shard of Health.Jungle Emblem of Health."
         "Home Shard of Strength.Home Emblem of Strength.Home Shard of Focus.Home Emblem of Focus."
         "Home Shard of Health.Home Emblem of Health.Bruiser Shard of Strength.Bruiser Emblem of Strength."
         "Bruiser Shard of Focus.Bruiser Emblem of Focus.Bruiser Shard of Health.Bruiser Emblem of Health").split(".")
ABILITY_TYPES = ["None", "AttackReset", "Damage", "AoeDamage", "Cc", "Heal", "Dash", "Shield"]
ROUND_FLAGS = {64: "RecallStarted", 128: "SubmitGemsStarted", 256: "RecallCancelled", 512: "SubmitGemsCancelled"}
GROUP_STATE = ["Unknown", "HasCamps", "WaitingForRewardClaim", "WaitingToRespawn"]
SNAP = 184760
TSNAP = 40152
OMNI = (2, 0xFFFFFFFF)  # spectator perspective


class Engine:
    def __init__(self, path=WASM):
        config = wasmtime.Config()
        config.consume_fuel = True
        self.engine = wasmtime.Engine(config)
        self.store = wasmtime.Store(self.engine)
        self.store.set_limits(memory_size=256 * 1024 * 1024, table_elements=100000,
                              instances=2, memories=2)
        self.store.set_fuel(500_000_000)
        w = wasmtime.WasiConfig()
        # No inherited environment, standard streams, or filesystem preopens.
        self.store.set_wasi(w)
        linker = wasmtime.Linker(self.engine)
        linker.define_wasi()
        mod = wasmtime.Module.from_file(self.engine, path)
        self.inst = linker.instantiate(self.store, mod)
        self.ex = self.inst.exports(self.store)
        self.mem = self.ex["memory"]
        self.ex["_initialize"](self.store)
        self.out = self.f("Engine_Malloc", 64)
        self.args = self.f("Engine_Malloc", 256)

    def f(self, name, *a):
        self.store.set_fuel(500_000_000)
        return self.ex[name](self.store, *a)

    def check(self, rc, what=""):
        if rc != 0:
            msg = self.cstr(self.f("GetLastErrorMsg"))
            raise RuntimeError(f"engine error {rc} in {what}: {msg}")

    def call(self, name, *a):
        self.check(self.f(name, *a), name)

    def read(self, ptr, n):
        if ptr < 0 or n < 0 or ptr + n > self.mem.data_len(self.store):
            raise ValueError("engine returned an out-of-bounds memory range")
        return self.mem.read(self.store, ptr, ptr + n)

    def write(self, ptr, b):
        self.mem.write(self.store, b, ptr)

    def u32(self, p): return struct.unpack("<I", self.read(p, 4))[0]
    def i32(self, p): return struct.unpack("<i", self.read(p, 4))[0]
    def u64(self, p): return struct.unpack("<Q", self.read(p, 8))[0]
    def f32(self, p): return struct.unpack("<f", self.read(p, 4))[0]

    def cstr(self, p):
        if not p:
            return ""
        out = bytearray()
        while len(out) < 16384:
            b = self.read(p, 64)
            z = b.find(b"\0")
            if z >= 0:
                out += b[:z]
                return out.decode("utf-8", "replace")
            out += b
            p += 64
        raise ValueError("unterminated engine error message")

    def malloc(self, n):
        return self.f("Engine_Malloc", n)


class Replay:
    def __init__(self, eng: Engine, data: bytes):
        self.e = eng
        n = len(data)
        p = eng.malloc(n)
        eng.write(p, data)
        eng.call("Game_FromReplayFileBytes", p, n, eng.out)
        self.h = eng.u64(eng.out)
        eng.f("Engine_Free", p)
        if eng.f("Game_Visualiser_SnapshotSize") != SNAP:
            raise ValueError("Unsupported engine snapshot ABI; expected 184696 bytes")
        self.snap = eng.malloc(SNAP)
        self.tsnap = eng.malloc(TSNAP)
        self.persp = eng.malloc(16)
        # map metadata
        eng.call("Game_GetBitboardMetadata", self.h, eng.out)
        self.W = eng.i32(eng.out)
        self.H = eng.i32(eng.out + 4)
        self.cw = eng.u32(eng.out + 8)
        self.ch = eng.u32(eng.out + 12)
        self.stride = eng.u32(eng.out + 16)
        eng.call("Game_GetNumChampsPerTeam", self.h, eng.out)
        self.team_size = eng.i32(eng.out)
        eng.call("Game_GetSymmetry", self.h, eng.out)
        self.symmetry = eng.i32(eng.out)
        eng.call("Game_GetTeamASpawnArea", self.h, eng.out)
        self.spawnA = struct.unpack("<4i", eng.read(eng.out, 16))

    # ---- terrain -------------------------------------------------------------
    def _board(self, fn):
        self.e.call(fn, self.h, self.e.out)
        ptr = self.e.u32(self.e.out)
        words = self.cw * self.ch
        raw = self.e.read(ptr, words * 8)
        tiles = set()
        for y in range(self.H):
            for x in range(self.W):
                w = (y // 8) * self.stride + x // 8
                bit = (y % 8) * 8 + x % 8
                v = int.from_bytes(raw[w * 8:w * 8 + 8], "little")
                if v >> bit & 1:
                    tiles.add((x, y))
        return tiles

    def terrain(self):
        return dict(walls=self._board("Game_GetWallBuffer"), bushes=self._board("Game_GetBushBuffer"),
                    holes=self._board("Game_GetHoleBuffer"), altars=self._board("Game_GetAltarBuffer"))

    def camp_groups(self):
        e = self.e
        e.call("Game_GetNumCamps", self.h, e.out)
        ncamps = e.u32(e.out)
        handles = {}
        camps = []
        for i in range(ncamps):
            e.call("Game_GetCampHandle", self.h, i, e.out)
            hd = e.u64(e.out)
            handles[hd] = i
            e.call("Game_GetCamp", self.h, hd, e.out)
            kind, x, y = struct.unpack("<iii", e.read(e.out, 12))
            camps.append((kind, x, y))
        e.call("Game_GetNumCampGroups", self.h, e.out)
        ng = e.u32(e.out)
        groups = []
        for g in range(ng):
            e.call("Game_GetCampGroupHandle", self.h, g, e.out)
            gh = e.u64(e.out)
            e.call("Game_GetCampGroup", self.h, gh, e.out)
            gem, gold, cd, nc = struct.unpack("<IIII", e.read(e.out, 16))
            members = []
            for k in range(nc):
                e.call("Game_GetCampGroupCamp", self.h, gh, k, e.out)
                members.append(handles.get(e.u64(e.out)))
            groups.append(dict(gems=gem, gold=gold, cooldown=cd, camps=[camps[m] for m in members if m is not None]))
        return camps, groups

    # ---- timeline ------------------------------------------------------------
    def main_timeline(self):
        self.e.call("Game_Visualiser_Timeline_GetMain", self.h, self.e.out)
        return self.e.u64(self.e.out)

    def timeline_size(self):
        tl = self.main_timeline()
        a = self.e.args
        self.e.call("Game_Visualiser_Timeline_Info", self.h, tl, a, a + 8, a + 16)
        return self.e.u32(a)

    def next(self):
        return self.e.f("Game_Visualiser_Timeline_ReplayNext", self.h, 1)

    def set_persp(self, team, champ):
        self.e.write(self.persp, struct.pack("<II", team, champ))

    def read_full(self, team=2, champ=0xFFFFFFFF):
        self.set_persp(team, champ)
        self.e.call("Game_Visualiser_ReadState", self.h, self.persp, self.snap)
        data = self.e.read(self.snap, SNAP)
        if struct.unpack_from('<II',data) != (1,SNAP):
            raise ValueError("Unsupported or corrupt visualiser snapshot header")
        return data

    def close(self):
        self.e.call('Game_Destroy', self.h)
        for p in (self.snap, self.tsnap, self.persp):
            self.e.f('Engine_Free',p)

    def read_timeline(self):
        self.e.call("Game_Visualiser_ReadTimelineState", self.h, self.tsnap)
        return self.e.read(self.tsnap, TSNAP)

def header(b, draft_off=179516):
    u = lambda o: struct.unpack_from("<I", b, o)[0]
    i = lambda o: struct.unpack_from("<i", b, o)[0]
    if (u(0),u(4)) != (1,SNAP):
        raise ValueError('Invalid snapshot ABI header')
    slot = u(108)
    drafts = [[i(draft_off + 12 + t * 20 + k * 4) for k in range(5)] for t in range(2)]
    return dict(round=u(8), turn=u(12), team=u(16), awaiting=u(20), phase=u(24), outcome=u(28),
                submitted=(struct.unpack_from("<Q", b, 32)[0], struct.unpack_from("<Q", b, 40)[0]),
                cur_champ=u(60),
                step=dict(phase=u(64), kind=u(68), actor=(u(72), u(76)), frm=(i(80), i(84)), to=(i(88), i(92)),
                          round=u(96), inpos=u(100), substep=u(104), slot=None if slot == 0xFFFFFFFF else slot,
                          atype=u(112) if slot != 0xFFFFFFFF else None, priority=i(116)),
                draft_rounds=u(draft_off), draft_counts=(u(draft_off + 4), u(draft_off + 8)), drafts=drafts)


def champions_full(b):
    """Parse champion records from a FULL snapshot (184696 bytes)."""
    u = lambda o: struct.unpack_from("<I", b, o)[0]
    i = lambda o: struct.unpack_from("<i", b, o)[0]
    f = lambda o: struct.unpack_from("<f", b, o)[0]
    n = u(120)
    if n > 16: raise ValueError('Invalid champion count')
    out = []
    for k in range(n):
        o = 140 + k * 456
        idx = u(o + 4)
        if idx >= 16: raise ValueError('Invalid champion entity index')
        known = u(o + 16)
        buffs = []
        mask = u(o + 40)
        for r in range(3):
            if mask & (1 << r):
                s = o + 44 + r * 16
                buffs.append((["Shield", "Stun", "EnhAuto"][r], i(s), round(f(s + 4), 1), (u(s + 8), u(s + 12))))
        stat = lambda q: dict(base=f(q), items=f(q + 4), bonus=f(q + 8), eff=f(q + 12))
        out.append(dict(
            idx=idx, team=u(o + 8), type=CHAMP[u(o + 12)] if u(o + 12) < len(CHAMP) else u(o + 12), known=known,
            pos=(i(o + 20), i(o + 24)), hp=f(o + 28), maxhp=f(o + 32), rflags=u(o + 36), buffs=buffs,
            cd=(u(o + 92), u(o + 96)), charges=(struct.unpack_from("<H", b, o + 100)[0], struct.unpack_from("<H", b, o + 102)[0]),
            abilities=(u(o + 104), u(o + 108)), arange=f(o + 112), focus=stat(o + 120), strength=stat(o + 136),
            alive=u(o + 224), respawn=i(o + 228), gold=u(o + 232), gems=u(o + 236),
            items=[u(o + 240 + 4 * j) for j in range(len(ITEMS))],
            ward_flag=u(o + 408), consumables=[u(o + 412 + 4 * j) for j in range(4)],
            ward=(u(o + 428), i(o + 432)), chan_timer=i(o + 436), chan_type=u(o + 440),
            priority=i(o + 444), next_priority=i(o + 448), draft_order=u(o + 452),
            submission_dur=i(184184 + idx * 4),
            earned_gold=u(184248 + idx * 16) + (u(184252 + idx * 16) << 32),
            treasure_gold=u(184256 + idx * 16) + (u(184260 + idx * 16) << 32),
            kda=(u(184504 + idx * 12), u(184504 + idx * 12 + 4), u(184504 + idx * 12 + 8))))
    return out


def camps_full(b):
    u = lambda o: struct.unpack_from("<I", b, o)[0]
    i = lambda o: struct.unpack_from("<i", b, o)[0]
    f = lambda o: struct.unpack_from("<f", b, o)[0]
    n = u(124)
    if n > 500: raise ValueError('Invalid camp count')
    out = []
    for k in range(n):
        o = 7436 + k * 232
        out.append(dict(idx=u(o + 4), team=u(o + 8), type=u(o + 12), known=u(o + 16), pos=(i(o + 20), i(o + 24)),
                        hp=f(o + 28), maxhp=f(o + 32), group=u(o + 224), awake=u(o + 228)))
    return out


def groups_full(b):
    u = lambda o: struct.unpack_from("<I", b, o)[0]
    i = lambda o: struct.unpack_from("<i", b, o)[0]
    n = u(128)
    if n > 500: raise ValueError('Invalid camp group count')
    out = []
    for k in range(n):
        o = 123436 + k * 20
        out.append(dict(state=u(o), reward=u(o + 4), capteam=u(o + 8), captimer=i(o + 12), respawn=i(o + 16)))
    return out


def wards_full(b):
    u = lambda o: struct.unpack_from("<I", b, o)[0]
    i = lambda o: struct.unpack_from("<i", b, o)[0]
    f = lambda o: struct.unpack_from("<f", b, o)[0]
    n = u(132)
    if n > 128: raise ValueError('Invalid ward count')
    out = []
    for k in range(n):
        o = 133436 + k * 40
        out.append(dict(entity=(u(o), u(o + 4)), owner=(u(o + 8), u(o + 12)), team=u(o + 16), pos=(i(o + 20), i(o + 24)),
                        timer=i(o + 28), radius=f(o + 32), disabled=b[o + 36]))
    return out


def control_wards_full(b):
    u = lambda o: struct.unpack_from("<I", b, o)[0]
    i = lambda o: struct.unpack_from("<i", b, o)[0]
    n = u(179568)
    if n > 128: raise ValueError('Invalid control ward count')
    out = []
    for k in range(n):
        o = 179572 + k * 36
        out.append(dict(entity=(u(o), u(o + 4)), owner=(u(o + 8), u(o + 12)), team=u(o + 16), pos=(i(o + 20), i(o + 24)),
                        timer=i(o + 28)))
    return out


def spells_full(b):
    u = lambda o: struct.unpack_from("<I", b, o)[0]
    i = lambda o: struct.unpack_from("<i", b, o)[0]
    f = lambda o: struct.unpack_from("<f", b, o)[0]
    n = u(136)
    if n > 1024: raise ValueError('Invalid spell count')
    out = []
    for k in range(n):
        o = 138556 + k * 40
        out.append(dict(entity=(u(o), u(o + 4)), owner=(u(o + 8), u(o + 12)), team=u(o + 16), type=u(o + 20),
                        pos=(i(o + 24), i(o + 28)), timer=i(o + 32), radius=f(o + 36)))
    return out
