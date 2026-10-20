"""Generate random symmetric CAMBC maps with the engine's own MapBuilder.

Usage: python gen.py OUT_DIR COUNT [SEED]
Design language copied from the official pool: walls with gaps, bush lanes/patches, holes, altar pairs,
every official camp mix (single wolves, wolf pairs/triples, elk, elk + gold, gem/gold clusters and 2x2
blocks, a central monolith with gem clusters), 3-5 champions, sizes 26-90.
The builder mirrors placements itself (checked); we place on one half and let it mirror, and only keep
maps that pass the engine's integrity check.
"""
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mapbuilder import Engine, MapBuilder, loads_in_game  # noqa: E402


def mirror(sym, w, h, x, y):
    if sym == 'horizontal':
        return w - 1 - x, y
    if sym == 'vertical':
        return x, h - 1 - y
    return w - 1 - x, h - 1 - y


def build_one(eng, rng, idx):
    sym = rng.choice(['horizontal', 'vertical', 'rotational', 'rotational'])
    if rng.random() < 0.15:
        w, h = (rng.randint(70, 90), rng.randint(26, 34)) if sym != 'vertical' else (rng.randint(26, 34), rng.randint(70, 90))
    else:
        w = h = rng.randint(28, 80)
    team = rng.choice([3, 3, 4, 4, 5, 5])
    if os.environ.get('GEN_TEAM'):  # v36: force one draft size (same rng draw kept for reproducibility)
        team = int(os.environ['GEN_TEAM'])
    mb = MapBuilder(eng, w, h, sym)
    mb.team_size(team)
    occupied = set()

    def free(x, y, pad=0):
        if not (1 <= x < w - 1 and 1 <= y < h - 1):
            return False
        for dx in range(-pad, pad + 1):
            for dy in range(-pad, pad + 1):
                if (x + dx, y + dy) in occupied:
                    return False
        mx, my = mirror(sym, w, h, x, y)
        if abs(mx - x) + abs(my - y) < 3:  # too close to the axis
            return False
        return True

    def mark(x, y):
        occupied.add((x, y))
        occupied.add(mirror(sym, w, h, x, y))

    # Our half: the side of the axis that holds team A's spawn.
    def in_half(x, y):
        if sym == 'horizontal':
            return x < w // 2 - 1
        if sym == 'vertical':
            return y < h // 2 - 1
        return x + y < (w + h) // 2 - 2

    # Spawn: a 3x3/4x4 box near a corner of our half.
    s = rng.choice([3, 3, 4])
    sx, sy = rng.randint(1, max(1, min(6, w // 5))), rng.randint(1, max(1, min(6, h // 5)))
    if sym == 'vertical' and rng.random() < 0.5:
        sx = rng.randint(2, w - s - 2)
    mb.spawn(sx, sy, sx + s - 1, sy + s - 1)
    for x in range(sx - 2, sx + s + 2):
        for y in range(sy - 2, sy + s + 2):
            mark(x, y)

    def try_place(fn, attempts=60):
        for _ in range(attempts):
            x, y = rng.randint(1, w - 2), rng.randint(1, h - 2)
            if in_half(x, y) and fn(x, y):
                return True
        return False

    # Walls: segments with gaps.
    n_walls = rng.randint(0, max(1, (w * h) // 500))
    for _ in range(n_walls):
        def seg(x, y):
            horiz = rng.random() < 0.5
            length = rng.randint(4, max(5, (w if horiz else h) // 3))
            thick = rng.choice([1, 1, 2])
            cells = []
            for i in range(length):
                for t in range(thick):
                    cx, cy = (x + i, y + t) if horiz else (x + t, y + i)
                    if not free(cx, cy, 1) or not in_half(cx, cy):
                        break
                    cells.append((cx, cy))
            if len(cells) < 4:
                return False
            for c in cells:
                mb.wall(*c)
                mark(*c)
            return True
        try_place(seg)
    # Bushes: lanes and patches.
    for _ in range(rng.randint(0, max(1, (w * h) // 700))):
        def patch(x, y):
            bw, bh = rng.randint(2, 8), rng.randint(1, 4)
            cells = [(x + i, y + j) for i in range(bw) for j in range(bh)]
            cells = [c for c in cells if free(*c) and in_half(*c)]
            if len(cells) < 3:
                return False
            for c in cells:
                mb.bush(*c)
                mark(*c)
            return True
        try_place(patch)
    # Holes.
    for _ in range(rng.randint(0, max(1, (w * h) // 1500))):
        def hole(x, y):
            hw, hh = rng.randint(1, 6), rng.randint(1, 2)
            cells = [(x + i, y + j) for i in range(hw) for j in range(hh)]
            cells = [c for c in cells if free(*c, 1) and in_half(*c)]
            if not cells:
                return False
            for c in cells:
                mb.hole(*c)
                mark(*c)
            return True
        try_place(hole)
    # Altars.
    for _ in range(rng.randint(1, max(1, (w * h) // 900 + 1))):
        def alt(x, y):
            if not free(x, y, 2):
                return False
            mb.altar(x, y)
            mark(x, y)
            return True
        try_place(alt)
    # Camps.
    shapes = [
        ('wolf', [(0, 0)]), ('wolf2', [(0, 0), (0, 1)]), ('wolf3', [(0, 0), (0, 1), (0, 2)]),
        ('elk', [(0, 0)]), ('elkgold', [(0, 0), (1, 0)]), ('gem', [(0, 0)]), ('gem2', [(0, 0), (1, 0)]),
        ('gemgold4', [(0, 0), (1, 0), (0, 1), (1, 1)]), ('gold', [(0, 0)]), ('gold3', [(0, 0), (0, 1), (0, 2)]),
    ]
    kinds = {'wolf': 'wolf', 'wolf2': 'wolf', 'wolf3': 'wolf', 'elk': 'elk', 'gem': 'gem', 'gem2': 'gem',
             'gold': 'gold', 'gold3': 'gold'}
    mix = rng.sample(shapes, rng.randint(2, 5))
    n_groups = rng.randint(3, max(4, (w * h) // 220))
    for _ in range(n_groups):
        name, cells = rng.choice(mix)

        def put(x, y):
            pts = [(x + dx, y + dy) for dx, dy in cells]
            if not all(free(px, py, 1) and in_half(px, py) for px, py in pts):
                return False
            for i, (px, py) in enumerate(pts):
                if name == 'elkgold':
                    k = 'elk' if i == 0 else 'gold'
                elif name == 'gemgold4':
                    k = 'gem' if i in (0, 3) else 'gold'
                else:
                    k = kinds[name]
                mb.camp(k, px, py)
            for px, py in pts:
                mark(px, py)
            return True
        try_place(put)
    # Optional central monolith (rotational maps only, like the official ones).
    if sym == 'rotational' and rng.random() < 0.25:
        cx, cy = w // 2 - 1, h // 2 - 1
        try:
            mb.camp('monolith', cx, cy)
            mb.camp('gem', cx + 1, cy)
        except RuntimeError:
            pass
    try:
        mb.verify()
        data = mb.encode()
    except RuntimeError:
        data = None
    mb.destroy()
    return data


def main():
    out, count = sys.argv[1], int(sys.argv[2])
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 2026
    os.makedirs(out, exist_ok=True)
    rng = random.Random(seed)
    eng = Engine()
    made = tries = 0
    while made < count and tries < count * 20:
        tries += 1
        data = build_one(eng, rng, made)
        if data and not loads_in_game(eng, data):
            data = None
        if data:
            with open(os.path.join(out, f'gen{seed % 1000:03d}_{made:04d}.map26a'), 'wb') as f:
                f.write(data)
            made += 1
    print(f'made {made} maps in {tries} tries -> {out}')


if __name__ == '__main__':
    main()
