#!/usr/bin/env python3
"""Generates assets/levels/1-1.lvl ... 1-8.lvl (World 1: Sunny Meadows).

Levels are drawn with small helpers instead of typed by hand, then checked
with levelcheck.reachable() before being written, so every coin, star,
friend and the exit is guaranteed to be reachable. The C++ unit tests run
the same checks on the shipped files.

Run from the PocketDash directory:  python3 tools/make_world1.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from levelcheck import check  # noqa: E402


class Grid:
    def __init__(self, w, h, fill="."):
        self.w, self.h = w, h
        self.sign_positions = []  # in the order signs were placed
        self.g = [[fill] * w for _ in range(h)]
        self.rect(0, 0, w, 1, "#")
        self.rect(0, h - 1, w, 1, "#")
        self.rect(0, 0, 1, h, "#")
        self.rect(w - 1, 0, 1, h, "#")

    def put(self, x, y, c):
        if c == "S":
            self.sign_positions.append((x, y))
        self.g[y][x] = c

    def text(self, x, y, s):
        for i, c in enumerate(s):
            self.put(x + i, y, c)

    def rect(self, x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.g[yy][xx] = c

    def box(self, x, y, w, h, c="#"):
        """Hollow rectangle (walls of a room)."""
        self.rect(x, y, w, 1, c)
        self.rect(x, y + h - 1, w, 1, c)
        self.rect(x, y, 1, h, c)
        self.rect(x + w - 1, y, 1, h, c)

    def rows(self):
        return ["".join(r) for r in self.g]


LEVELS = []


def level(**header):
    """Each level function returns (grid, sign texts in the order the signs
    were placed). The file format lists texts in reading order (row by row),
    so they are re-sorted by position here."""
    def wrap(fn):
        grid, signs = fn()
        if len(signs) != len(grid.sign_positions):
            raise SystemExit(f"{header['id']}: {len(grid.sign_positions)} signs placed, {len(signs)} texts")
        ordered = [t for _, t in sorted(zip(grid.sign_positions, signs), key=lambda p: (p[0][1], p[0][0]))]
        LEVELS.append((header, grid.rows(), ordered))
        return fn
    return wrap


# --- 1-1 Welcome Meadow: the tutorial --------------------------------------
@level(id="1-1", name="WELCOME MEADOW", objective="exit")
def welcome():
    g = Grid(40, 15)
    g.put(2, 7, "P")
    g.put(4, 6, "S")
    for x in range(6, 10):
        g.put(x, 7, "c")
    for (x, y) in [(3, 2), (8, 3), (5, 12), (9, 11), (2, 11)]:
        g.put(x, y, "T")
    # Thorn gate: hop over.
    g.rect(12, 1, 1, 13, "#")
    g.rect(12, 5, 1, 5, "^")
    g.put(10, 6, "S")
    for x in range(14, 17):
        g.put(x, 7, "c")
    g.put(14, 2, "*")  # first star: just explore
    g.put(15, 11, "2")  # a shield to try
    # Crate gate: dash through.
    g.rect(19, 1, 1, 13, "#")
    g.rect(19, 6, 1, 3, "x")
    g.put(17, 6, "S")
    g.put(21, 12, "*")  # second star: tucked in a corner
    # Enemies: stomp or dash.
    g.put(21, 5, "S")
    g.put(22, 9, "C")
    g.put(26, 4, "s")
    g.put(25, 10, "b")
    for x in range(23, 31, 2):
        g.put(x, 7, "c")
    g.put(29, 12, "h")
    for (x, y) in [(24, 2), (30, 11), (28, 2)]:
        g.put(x, y, "T")
    # Last stretch: a secret hedge room hides the third star.
    g.rect(32, 1, 1, 13, "#")
    g.put(32, 7, ".")
    g.box(33, 0, 7, 5)
    g.put(36, 4, "%")
    g.put(37, 2, "*")
    g.put(34, 6, "S")
    g.put(37, 9, "E")
    g.put(35, 11, "c")
    g.put(36, 11, "c")
    return g, [
        "WELCOME TO POCKET DASH! USE THE D-PAD TO MOVE. PRESS Y NEXT TO SIGNS TO READ THEM.",
        "OUCH, THORNS! PRESS A TO HOP RIGHT OVER THEM.",
        "WOODEN CRATES BLOCK THE WAY. PRESS B TO DASH AND SMASH THROUGH!",
        "HOP ONTO ENEMIES TO STOMP THEM, OR DASH INTO THEM. TOUCH A FLAG TO SAVE YOUR PLACE.",
        "PSST... SOME HEDGES ARE SECRET PASSAGES. THERE ARE 3 STARS IN EVERY LEVEL!",
    ]


# --- 1-2 River Run: rafts, bridges and streams -------------------------------
@level(id="1-2", name="RIVER RUN", objective="exit")
def river():
    g = Grid(46, 18)
    g.put(2, 8, "P")
    g.put(4, 7, "S")
    # A one-tile stream to hop.
    g.rect(7, 1, 1, 16, "~")
    for y in (4, 8, 12):
        g.put(5, y, "c")
        g.put(9, y, "c")
    # River 1 (cols 11-14): a raft shuttles across row 8, a bridge at row 15.
    g.rect(11, 1, 4, 16, "~")
    g.put(11, 8, "R")
    g.rect(11, 15, 4, 1, "=")
    g.put(9, 7, "S")
    g.put(12, 3, ".")  # a little islet one hop from the bank...
    g.put(13, 3, "*")  # ...with a star on it
    # Bank 2 with a checkpoint and beetles.
    g.put(17, 8, "C")
    g.put(19, 3, "B")
    g.put(18, 13, "b")
    for x in range(16, 22):
        g.put(x, 10, "c")
    g.rect(22, 1, 1, 16, "#")
    g.put(22, 8, ".")
    # Bank 3: a pond crossed by an up/down raft leads to a hidden star.
    g.rect(23, 12, 3, 3, "~")
    g.put(24, 12, "V")
    g.put(25, 10, "S")
    g.put(24, 16, "*")
    g.put(23, 9, "s")
    # River 2 (cols 26-31): board the raft up on row 4.
    g.rect(26, 1, 6, 16, "~")
    g.put(26, 4, "R")
    for y in range(2, 7):
        g.put(24, y, "c")
    # Bank 4 and the stream maze to the exit.
    g.put(33, 3, "h")
    for y in range(10, 15):
        g.put(33, y, "c")
    g.rect(35, 1, 1, 16, "#")
    g.put(35, 5, ".")
    g.put(35, 13, ".")
    g.rect(36, 1, 1, 16, "~")
    g.rect(40, 1, 1, 12, "#")
    g.put(40, 6, "%")
    g.put(42, 3, "*")
    g.put(43, 14, "E")
    g.put(38, 15, "1")
    for x in range(37, 40):
        g.put(x, 9, "c")
    return g, [
        "RIVERS AHEAD! YOU CAN HOP OVER A NARROW STREAM.",
        "STAND ON THE RAFT AND IT WILL CARRY YOU ACROSS.",
        "THIS RAFT GOES UP AND DOWN. HOP ON AND RIDE IT TO THE OTHER SIDE!",
    ]


# --- 1-3 Coin Forest: collect 40 coins ---------------------------------------
@level(id="1-3", name="COIN FOREST", objective="coins", goal="40")
def forest():
    g = Grid(42, 22)
    g.put(3, 3, "P")
    g.put(5, 3, "S")
    # Tree clumps make a forest maze.
    clumps = [(8, 2, 2, 6), (14, 6, 6, 2), (8, 11, 8, 2), (22, 2, 2, 8), (26, 12, 2, 7), (30, 4, 6, 2),
              (32, 9, 2, 6), (16, 15, 6, 2), (4, 15, 2, 4), (36, 15, 4, 2)]
    for (x, y, w, h) in clumps:
        g.rect(x, y, w, h, "T")
    # Coin trails (60 coins, 40 needed).
    trails = [(3, 5, 1, 0, 4), (11, 3, 1, 0, 9), (11, 9, 1, 0, 9), (20, 4, 0, 1, 6), (25, 3, 1, 0, 4),
              (29, 7, 1, 0, 9), (3, 9, 0, 1, 5), (7, 19, 1, 0, 8), (24, 10, 0, 1, 8), (30, 17, 1, 0, 6)]
    for (x, y, dx, dy, n) in trails:
        for i in range(n):
            if g.g[y + dy * i][x + dx * i] == ".":
                g.put(x + dx * i, y + dy * i, "c")
    g.put(13, 13, "3")  # magnet
    g.put(19, 13, "C")
    g.put(17, 4, "s")
    g.put(28, 15, "s")
    g.put(14, 18, "b")
    g.put(34, 7, "B")
    g.put(37, 11, "m")
    g.put(2, 19, "h")
    # Stars.
    g.box(36, 1, 5, 5)
    g.put(38, 5, ":")  # tiny gap...
    g.put(38, 3, "*")
    g.put(33, 18, "6")  # ...opened by Tiny Mode
    g.put(9, 1, "*")
    g.rect(1, 12, 3, 1, "#")
    g.put(1, 13, "*")
    g.put(39, 19, "E")
    return g, ["THIS FOREST IS FULL OF COINS! COLLECT 40 AND THE FLAG WILL OPEN."]


# --- 1-4 Lost Friends: rescue three chicks ---------------------------------------
@level(id="1-4", name="LOST FRIENDS", objective="rescue")
def friends():
    g = Grid(40, 22)
    g.put(19, 19, "P")
    g.put(17, 19, "S")
    # Friend 1: out in the open meadow.
    g.put(6, 15, "f")
    g.put(9, 13, "s")
    # Friend 2: across the pond (raft).
    g.rect(24, 9, 8, 5, "~")
    g.put(24, 11, "R")
    g.put(34, 11, "f")
    g.box(33, 8, 6, 7)
    g.put(33, 11, ".")
    g.put(36, 9, "*")
    # Friend 3: in a locked pen; the key is up north.
    g.box(3, 2, 9, 6)
    g.rect(7, 7, 1, 1, "L")
    g.put(6, 4, "f")
    g.put(9, 4, "*")
    g.put(25, 2, "K")
    g.put(22, 4, "m")
    g.put(28, 5, "b")
    g.put(16, 6, "S")
    g.put(18, 11, "C")
    for x in range(12, 22, 2):
        g.put(x, 17, "c")
    for y in range(2, 8):
        g.put(33, y, "c")
    for (x, y) in [(14, 3), (2, 11), (12, 12), (30, 17), (36, 18), (22, 15)]:
        g.put(x, y, "T")
    g.put(37, 2, "*")
    g.put(36, 4, "h")
    g.put(19, 2, "E")
    g.rect(16, 1, 1, 4, "#")
    g.rect(22, 1, 1, 4, "#")
    g.put(16, 3, "%")
    return g, [
        "THREE BABY CHICKS ARE LOST! FIND THEM AND PRESS Y TO SEND THEM HOME.",
        "ONE CHICK IS LOCKED IN THE OLD PEN. A KEY OPENS ANY LOCKED GATE.",
    ]


# --- 1-5 Dash Valley: crates and wide thorn patches ------------------------------
@level(id="1-5", name="DASH VALLEY", objective="exit")
def dash():
    g = Grid(50, 15)
    g.put(2, 7, "P")
    g.put(4, 6, "S")
    # Crate walls.
    for x in (8, 15):
        g.rect(x, 1, 1, 13, "#")
        g.rect(x, 5, 1, 5, "x")
    for x in range(10, 14):
        g.put(x, 7, "c")
    # Two-tile thorn strips: hop, then dash in the air.
    g.put(17, 6, "S")
    g.rect(20, 1, 2, 13, "^")
    g.rect(27, 1, 2, 13, "^")
    g.put(24, 7, "C")
    g.put(24, 3, "s")
    for y in (4, 10):
        g.put(23, y, "c")
        g.put(25, y, "c")
    # Super Dash corridor: a long thorn run with a star across it.
    g.put(30, 2, "4")
    g.rect(31, 1, 1, 13, "#")
    g.put(31, 7, ".")
    # A pocket behind four tiles of thorns: only a Super Dash (hop + dash) makes it.
    g.rect(32, 4, 6, 1, "#")
    g.put(32, 4, ".")
    g.rect(33, 1, 4, 3, "^")
    g.put(37, 2, "*")
    # Crate maze to the exit.
    for (x, y) in [(35, 6), (35, 7), (35, 8), (40, 9), (40, 10), (40, 11), (44, 4), (44, 5), (44, 6)]:
        g.put(x, y, "x")
    g.rect(38, 1, 1, 7, "#")
    g.rect(42, 8, 1, 6, "#")
    g.put(39, 12, "b")
    g.put(46, 10, "m")
    g.put(33, 12, "*")
    g.rect(32, 10, 3, 1, "x")
    g.rect(34, 11, 1, 3, "x")
    g.box(44, 0, 6, 4)
    g.put(46, 3, "%")
    g.put(47, 1, "*")
    g.put(47, 7, "E")
    g.put(10, 12, "h")
    return g, [
        "DASH VALLEY! PRESS B TO DASH THROUGH CRATES.",
        "WIDE THORNS? HOP WITH A, THEN DASH WITH B WHILE IN THE AIR TO FLY FAR!",
    ]


# --- 1-6 Hidden Garden: find all three stars ----------------------------------
@level(id="1-6", name="HIDDEN GARDEN", objective="stars")
def garden():
    g = Grid(42, 24)
    g.put(20, 21, "P")
    g.put(18, 21, "S")
    # Garden hedges.
    g.box(2, 2, 12, 9)        # west garden (secret entrance)
    g.put(13, 6, "%")
    g.put(7, 5, "*")
    g.put(5, 8, "g")
    g.box(28, 2, 12, 9)       # east garden (locked)
    g.rect(28, 6, 1, 2, "L")
    g.put(34, 5, "*")
    g.put(36, 8, "h")
    g.box(15, 2, 11, 7)       # north garden (tiny gap)
    g.put(20, 8, ":")
    g.put(20, 4, "*")
    g.put(17, 4, "c")
    g.put(23, 4, "c")
    # Keys and power-ups.
    g.put(4, 18, "K")
    g.put(37, 18, "6")
    g.put(10, 14, "7")
    # The key sits in a walled pocket: only Giant Mode can smash the boulder in.
    g.rect(1, 16, 6, 1, "#")
    g.rect(6, 16, 1, 7, "#")
    g.put(6, 19, "X")
    # Enemies and coins.
    g.put(12, 18, "s")
    g.put(30, 15, "s")
    g.put(24, 13, "b")
    g.put(33, 19, "m")
    for x in range(15, 27, 2):
        g.put(x, 11, "c")
    for y in range(13, 20, 2):
        g.put(26, y, "c")
    g.put(20, 15, "C")
    for (x, y) in [(15, 17), (9, 21), (35, 13), (27, 21), (39, 21)]:
        g.put(x, y, "T")
    g.put(40, 12, "E")
    return g, ["THE GARDEN HIDES THREE STARS. FIND THEM ALL TO OPEN THE FLAG. LOOK FOR KEYS AND SECRET HEDGES!"]


# --- 1-7 Star Sprint: beat the clock -----------------------------------------
@level(id="1-7", name="STAR SPRINT", objective="exit", time_limit="75")
def sprint():
    g = Grid(64, 13)
    g.put(2, 6, "P")
    g.put(4, 5, "S")
    g.put(6, 6, "1")  # speed shoes right away
    # A winding track: hedge baffles force a zig-zag.
    for i, x in enumerate(range(10, 58, 6)):
        if i % 2 == 0:
            g.rect(x, 1, 1, 8, "#")
        else:
            g.rect(x, 4, 1, 8, "#")
    for x in range(8, 58, 2):
        y = 10 if (x // 6) % 2 == 1 else 2
        if g.g[y][x] == ".":
            g.put(x, y, "c")
    g.put(13, 6, "s")
    g.put(25, 6, "b")
    g.put(37, 6, "s")
    g.put(49, 6, "m")
    g.put(31, 6, "C")
    g.rect(19, 1, 2, 1, "^")
    g.put(21, 1, "*")
    g.put(45, 11, "*")
    g.rect(44, 11, 1, 1, "^")
    g.box(58, 0, 6, 5)
    g.put(60, 4, "%")
    g.put(61, 2, "*")
    g.put(61, 9, "E")
    g.put(34, 10, "5")
    return g, ["STAR SPRINT! REACH THE FLAG BEFORE THE CLOCK RUNS OUT. GRAB THE SPEED SHOES!"]


# --- 1-8 Meadow Guardian: the boss arena (boss arrives in Phase 7) -----------
@level(id="1-8", name="MEADOW GUARDIAN", objective="exit")
def guardian():
    g = Grid(30, 20)
    g.put(15, 17, "P")
    g.put(13, 17, "S")
    g.box(6, 4, 18, 12, "T")
    g.put(15, 15, ".")
    g.put(15, 4, ".")
    for (x, y) in [(10, 8), (19, 8), (10, 12), (19, 12)]:
        g.put(x, y, "o")
    g.put(2, 2, "*")
    g.put(27, 2, "*")
    g.put(2, 17, "*")
    g.put(26, 17, "h")
    g.put(15, 2, "E")
    for x in range(12, 19, 2):
        g.put(x, 10, "c")
    return g, ["THE MEADOW GUARDIAN SLEEPS IN THE CLEARING... (THE BOSS WAKES UP IN A FUTURE UPDATE!)"]


def main():
    out_dir = os.path.join(os.path.dirname(__file__), "..", "assets", "levels")
    problems = []
    for header, rows, signs in LEVELS:
        problems += check(header["id"], rows)
        n_signs = "".join(rows).count("S")
        if n_signs != len(signs):
            problems.append(f"{header['id']}: {n_signs} signs on map, {len(signs)} texts")
    if problems:
        print("\n".join(problems))
        sys.exit(1)
    for header, rows, signs in LEVELS:
        path = os.path.join(out_dir, header["id"] + ".lvl")
        with open(path, "w") as f:
            f.write("# Pocket Dash level - generated by tools/make_world1.py\n")
            for k in ("id", "name", "objective", "goal", "time_limit"):
                if k in header:
                    f.write(f"{k}={header[k]}\n")
            f.write("world=1\n[map]\n")
            f.write("\n".join(rows) + "\n")
            if signs:
                f.write("[signs]\n" + "\n".join(signs) + "\n")
        print(f"wrote {path} ({len(rows[0])}x{len(rows)})")


if __name__ == "__main__":
    main()
