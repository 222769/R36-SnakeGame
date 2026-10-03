"""Shared reachability model for Pocket Dash level design (mirrors the C++ level test).

A tile is standable unless it is solid or dangerous. From a standable tile
the hero may walk to a neighbour, or hop (+dash) straight over 1-2 danger
tiles to a standable tile beyond (1-4 with Super Dash, 4, in the level).
Crates are smashable by a dash. Tiny gaps,
boulders and locks open only if the level provides a Tiny power-up (6),
Giant (7) or at least one key (K). Water on a raft's track counts as
standable (the raft carries you).
"""
SOLID = set("#To")
DANGER = set("~^RV")
ENTITY = set("chCkrEsbBm*g12345678KfS")


def reachable(rows):
    h, w = len(rows), len(rows[0])
    text = "".join(rows)
    tiny, giant, key = "6" in text, "7" in text, "K" in text
    max_gap = 4 if "4" in text else 2
    raft = set()
    for y in range(h):
        for x in range(w):
            c = rows[y][x]
            if c in "RV":
                dx, dy = (1, 0) if c == "R" else (0, 1)
                for sgn in (1, -1):
                    cx, cy = x, y
                    while 0 <= cx < w and 0 <= cy < h and rows[cy][cx] in "~RV":
                        raft.add((cx, cy))
                        cx += dx * sgn
                        cy += dy * sgn

    def standable(x, y):
        if not (0 <= x < w and 0 <= y < h):
            return False
        c = rows[y][x]
        if (x, y) in raft:
            return True
        if c in SOLID or c in DANGER:
            return False
        if c == ":" and not tiny:
            return False
        if c == "X" and not giant:
            return False
        if c == "L" and not key:
            return False
        return True

    start = next((x, y) for y in range(h) for x in range(w) if rows[y][x] == "P")
    seen = {start}
    stack = [start]
    while stack:
        x, y = stack.pop()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            for n in range(0, max_gap + 1):  # walk (n=0), or cross 1..max_gap danger tiles
                gap_ok = all(rows[y + dy * k][x + dx * k] in DANGER and (x + dx * k, y + dy * k) not in raft
                             for k in range(1, n + 1)
                             if 0 <= y + dy * k < h and 0 <= x + dx * k < w)
                tx, ty = x + dx * (n + 1), y + dy * (n + 1)
                if n > 0 and not gap_ok:
                    break
                if standable(tx, ty) and (tx, ty) not in seen:
                    seen.add((tx, ty))
                    stack.append((tx, ty))
                if n == 0 and standable(tx, ty):
                    break
    return seen


def check(name, rows):
    w = len(rows[0])
    bad = [i for i, r in enumerate(rows) if len(r) != w]
    if bad:
        return [f"{name}: ragged rows {bad}"]
    seen = reachable(rows)
    issues = []
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c in ENTITY and (x, y) not in seen:
                issues.append(f"{name}: '{c}' at ({x},{y}) unreachable")
    if "".join(rows).count("*") != 3:
        issues.append(f"{name}: needs exactly 3 stars")
    return issues
