"""Level 3 in the menu, CORONA RUN: a long timed parkour path with side branches (writes
assets/levels/corona_run.txt and the bot route tests/corona_route.txt).

Helios Yard is an open yard to explore. Corona Run is a path: nine main strands wind round the
76 x 76 room twice, climbing from stand height 4 to 12 and back down, with four short side
branches (grey-teal floor) that hold four of the ten cores. Below everything is the void (floor
paint 9): landing on it costs one armour cell and returns the courier to the last checkpoint.
There are only four checkpoints, so a mistake costs a long stretch.

Main path (sand and white floors)
  S1  east along the north edge:   gaps, a hurdle, a low bar, rising zigzag stones, a shock strip,
                                   a five-cell sprint gap, a sweeper
  S2  south down the east side:    junction P (B2)                                  checkpoint 1
  N1  west through the north:      a chain of two jump pads (6 -> 9 -> 12), a zigzag beam, a
                                   six-cell long jump two blocks down, hurdles and a low bar
  N2  south down the west side:    1x1 stones shocked in turn (hop on the beat)       checkpoint 2
  S3  east through the middle:     a pillar climb, a shocked sky beam crossed by two long
                                   sweepers, fork F (B3), a sliding block, a sprint gap, a shocked
                                   crawl tunnel
  S4  south down the east side:    a descent over dropping zigzag stones             checkpoint 3
  S5  west along the south:        a rolling shock wave, a sweeper, junction J, the main stretch
                                   or the B5 loop, junction K, hurdles and a low bar
  S6  south down the west side:    a sliding block, a sprint gap, a shock strip      checkpoint 4
  S7  east along the south edge:   a jump pad onto a wall top (a core on it), a sweeper, a flat
                                   six-cell sprint gap, a sliding block, a shock strip, then over
                                   the west wall of the court into the exit court
Side branches (one or two cores each, each a different kind of parkour)
  B1  off S1:  a jump pad beside the path lifts the courier to a perch; drop back down
  B2  off P:   1x1 stepping stones climb east to a perch; hop back the same way
  B3  off F:   a beam with a shock strip, then west beside S3 to two platforms; back the same way
  B5  off J:   a loop round the south side over zigzag stones, rejoining at K
Hazards that cover two routes
  P   one shock field covers the whole junction: on the way down S2, into B2 and back out of it
  XA, XB  two long sweepers cross both the S3 sky beam and the B3 beam beside it, so each is met
      three times: east on S3, then west and east again on B3 (while one beam is swept, the
      other is clear)
  K   one shock field covers the junction where the B5 loop and the S5 main stretch meet

Heights: the floor is y = 0, so a block filled through layer k is stood on at height k + 1 (a path
"at S" has its top block on layer S - 1). The courier jumps about 1.8 high, roughly 3.5 cells
walking and 5.7 sprinting (more with its 0.7 radius); a jump pad lifts it 4.3.
"""
from pathlib import Path as FsPath

ROOT = FsPath(__file__).resolve().parents[1]
LO, HI = 4, 80          # room 4..79 in x and z
HEIGHT, WALL = 15, 16
TIME = 300
WHITE, LOW, GUIDE, BEACON, BRANCH, SAND, SLATE, VOID = '#', '=', '4', '5', '6', '7', '8', '9'
E, W, UP, DN = (1, 0), (-1, 0), (0, 1), (0, -1)   # UP is +z, DN is -z

cells = {}              # (x, y, z) -> glyph
items = []              # level file lines (checkpoints, jump pads)
route = []              # bot route lines (tests/corona_route.txt)
traps = []              # (x0, z0, x1, z1, offset, level)
movers = []             # ((ax, az), (bx, bz), period, offset, level)
cores = []
ROUTE = [True]          # route recording (off for the stretch the bot does not walk)

def emit(line):
    if ROUTE[0]: route.append(line)

def go(p, mode='s'): emit(f'go {p[0]:.2f} {p[1]:.2f} {mode}')

def column(x, z, stand, glyph):
    """Fill the column at (x, z) so it is stood on at height `stand`."""
    for y in range(1, stand):
        cells[(x, y, z)] = glyph

def box(x0, x1, z0, z1, stand, glyph):
    for x in range(min(x0, x1), max(x0, x1) + 1):
        for z in range(min(z0, z1), max(z0, z1) + 1):
            column(x, z, stand, glyph)

def trim(x, z, stand, glyph=GUIDE):
    """Recolour the walkable top block (jump lips, stones)."""
    if (x, stand - 1, z) in cells:
        cells[(x, stand - 1, z)] = glyph

def add_trap(cs, offset, stand):
    xs = [c[0] for c in cs]; zs = [c[1] for c in cs]
    traps.append((min(xs), min(zs), max(xs) + 1, max(zs) + 1, offset, stand))
    return len(traps) - 1

def add_mover(a, b, period, offset, stand):
    movers.append((a, b, period, offset, stand))
    return len(movers) - 1

def add_core(p):
    cores.append(p); go(p); emit('core')

def checkpoint(pylon, respawn):
    items.append(f'checkpoint {pylon[0]} {pylon[1]} {respawn[0]} {respawn[1]}')

class Run:
    """A path laid like a turtle: `head` is the next cell on the centre line, `d` the direction.
    Lateral offsets (lat) are measured along p = (-dz, dx). Every piece lays its cells, records the
    bot's route through it and moves the head on."""
    def __init__(s, x, z, d, stand, glyph, width=3):
        s.x, s.z, s.d, s.stand, s.glyph, s.width = x, z, d, stand, glyph, width
        s.after_gap = False
        s.last = s.pt(-1)

    @property
    def p(s): return (-s.d[1], s.d[0])
    def cell(s, k, lat=0): return (s.x + s.d[0] * k + s.p[0] * lat, s.z + s.d[1] * k + s.p[1] * lat)
    def pt(s, k, lat=0.): return (s.x + .5 + s.d[0] * k + s.p[0] * lat, s.z + .5 + s.d[1] * k + s.p[1] * lat)
    def lats(s, w): return list(range(-(w // 2), w // 2 + 1))
    def back(s, p, dist): return (p[0] - s.d[0] * dist, p[1] - s.d[1] * dist)
    def axis(s): return 'x' if s.p[0] != 0 else 'z'          # the axis a crossing sweeper moves on
    def lane(s): return s.x + .5 if s.p[0] != 0 else s.z + .5   # the centre line on that axis

    def lay(s, n, stand=None, width=None, glyph=None):
        if stand is not None: s.stand = stand
        if width is not None: s.width = width
        out = []
        for k in range(n):
            for lat in s.lats(s.width):
                c = s.cell(k, lat); column(*c, s.stand, glyph or s.glyph); out.append(c)
        s.last = s.pt(n - 1); s.after_gap = False; s.prev_trap = False
        return out

    def advance(s, n): s.x += s.d[0] * n; s.z += s.d[1] * n

    def wait_spot(s, back):
        """Where to stand before the next piece: on the floor just before it, or on the lip
        before the gap that leads to it (plus the time the gap takes to cross)."""
        if s.after_gap: return s.back(s.last, .3), .35
        return s.pt(-back), 0.

    # ---- pieces ------------------------------------------------------------------------------
    def run(s, n, stand=None, width=None):
        s.lay(n, stand, width); s.advance(n)

    def beam(s, n, stand=None):
        s.run(n, stand, 1)

    def gap(s, n):
        for lat in s.lats(s.width): trim(*s.cell(-1, lat), s.stand)
        s.advance(n); s.after_gap = True

    def hurdle(s, n=1):
        for (x, z) in s.lay(n): cells[(x, s.stand, z)] = SLATE
        s.advance(n)

    def traps(s, n, offset, stand=None, width=None):
        """A shock strip. Strips split only by gaps form a wave (offsets chosen so each turns safe
        as the courier reaches it): the wave is taken in one go, starting as the first strip
        turns safe, never waiting on a strip's lip, which is itself shocked."""
        chained = s.after_gap and getattr(s, 'prev_trap', False)
        spot, extra = s.wait_spot(1.4)
        t = add_trap(s.lay(n, stand, width), offset, s.stand)
        if chained:
            # Walk the wave (a sprint jump carries the courier over a strip into the next gap).
            route[s.wave_wait] = route[s.wave_wait].rsplit(' ', 1)[0] + ' 2.60'
            go(s.pt((n - 1) / 2), 'w')
        else:
            go(spot); emit(f'wtrap {t} {n / 6.0 + .45 + extra:.2f}'); s.wave_wait = len(route) - 1
        s.prev_trap = True
        s.advance(n); return t

    def sweep(s, n, period, offset, near, far, stand=None, width=None):
        """A block sweeping across the path at its middle cell, from `near` to `far` (lateral)."""
        k = n // 2
        spot = s.back(s.last, .3) if s.after_gap else s.pt(k - 2.3)
        s.lay(n, stand, width)
        m = add_mover(s.pt(k, near), s.pt(k, far), period, offset, s.stand)
        go(spot); emit(f'wmover {m} {s.axis()} {s.lane():.2f} {2.95 if s.width == 3 else 2.15}')
        s.advance(n); return m

    def slider(s, n, period, offset, stand=None, width=None):
        """A block sliding back and forth along the path: run at it and jump over it."""
        spot, _ = s.wait_spot(1.9)   # clear of the block's near end (half width plus our radius)
        s.lay(n, stand, width)
        m = add_mover(s.pt(0), s.pt(n - 1), period, offset, s.stand)
        go(spot); emit(f'jmover {m}')
        s.advance(n); return m

    def tunnel(s, n, offset=None, stand=None):
        """A crawl tunnel (one block of headroom), optionally with a shock strip inside."""
        spot, _ = s.wait_spot(1.4)
        cs = s.lay(n, stand, 3)
        for (x, z) in cs: cells[(x, s.stand + 1, z)] = WHITE
        for k in range(n):
            x, z = s.cell(k, 1); cells[(x, s.stand + 2, z)] = WHITE
        go(spot)
        if offset is not None:
            t = add_trap(cs, offset, s.stand); emit(f'wtrap {t} {n / 2.4 + .5:.2f}')
        go(s.pt(n - 1 + 1.2), 'c')
        s.advance(n)

    def core(s, n=3, stand=None, width=None):
        s.lay(n, stand, width); add_core(s.pt(n // 2)); s.advance(n)

    def steps(s, parts):
        """Pillars at their own heights split by gaps: (length, stand) or (length, None) for a gap.
        Taken at a walk, landing on each pillar (a sprint carries the courier off a short one)."""
        for (n, st) in parts:
            if st is None: s.gap(n)
            else: s.lay(n, st); go(s.pt((n - 1) / 2), 'w'); s.advance(n)

    def stones(s, parts):
        """Single 1x1 stones, each `gap` cells on and at lateral `lat`, hopped one by one."""
        if not s.after_gap: go(s.last)
        for (g, lat, st) in parts:
            s.advance(g)
            c = s.cell(0, lat); column(*c, st, s.glyph); trim(*c, st)
            go(s.pt(0, lat), 'w')
            s.last = s.pt(0, lat); s.stand = st
            s.advance(1)
        s.after_gap = True

    def lowbar(s, n=1):
        """A bar with one block of headroom under it (too high to jump): crouch under it."""
        for (x, z) in s.lay(n): cells[(x, s.stand + 1, z)] = SLATE
        go(s.pt(-1.2)); go(s.pt(n - 1 + 1.2), 'c')
        s.advance(n)

    def zigzag(s, lats):
        """A one-cell beam that jogs sideways: lats[k] is its lateral offset at step k, and a jog
        fills the cells between, so the beam turns a corner twice at every jog."""
        prev = lats[0]
        for k, lat in enumerate(lats):
            lo, hi = sorted((prev, lat))
            for l in range(lo, hi + 1): column(*s.cell(k, l), s.stand, s.glyph)
            if lat != prev: go(s.pt(k, prev), 'w'); go(s.pt(k, lat), 'w')
            prev = lat
        s.last = s.pt(len(lats) - 1, lats[-1]); s.after_gap = False; s.width = 1
        s.advance(len(lats))

    def shockstones(s, parts):
        """1x1 stones, each with its own shock plate; neighbours alternate (offsets 0 and 3), so
        the courier hops on the beat, onto each stone as it turns safe and the last one warns."""
        if not s.after_gap: go(s.last)
        for i, (g, lat, st) in enumerate(parts):
            s.advance(g)
            c = s.cell(0, lat); column(*c, st, s.glyph)
            t = add_trap([c], 3.0 * (i % 2), st)
            emit(f'wtrap {t} 2.00'); go(s.pt(0, lat), 'w')
            s.last = s.pt(0, lat); s.stand = st
            s.advance(1)
        s.after_gap = True

    def turn(s, nd, stand=None, size=3):
        """A square corner, then carry on in direction nd. Returns the corner's centre cell."""
        h = size // 2
        if stand is not None: s.stand = stand
        centre = s.cell(h)
        for k in range(size):
            for lat in range(-h, h + 1):
                column(*s.cell(k, lat), s.stand, s.glyph)
        cp = (centre[0] + .5, centre[1] + .5)
        go(cp)
        outer = (centre[0] + s.d[0] - nd[0], centre[1] + s.d[1] - nd[1])
        s.d = nd
        s.x, s.z = centre[0] + nd[0] * (h + 1), centre[1] + nd[1] * (h + 1)
        s.last = cp; s.after_gap = False; s.width = size
        s.corner = (cp, (outer[0] + .5, outer[1] + .5))
        return centre

def corner_checkpoint(run):
    cp, pylon = run.corner
    checkpoint(pylon, cp)

# =============================================================================================
# Start platform and S1 (east along the north edge, S4 -> 6): warm-up, then B1.
box(6, 12, 73, 75, 4, SAND)
s1 = Run(13, 74, E, 4, SAND)
s1.run(3); s1.gap(2); s1.run(3); s1.hurdle(); s1.run(2); s1.lowbar(); s1.run(2)
s1.stones([(1, -1, 5), (1, 1, 5), (1, -1, 6), (1, 1, 6)])     # rising zigzag
s1.gap(1)
s1.core(3, 6)                                                 # core 1
s1.traps(4, 0.0)
s1.run(3)
# B1: a jump pad on the inner edge lifts the courier to a perch with a core; then drop back down.
bx = s1.x - 3
items.append(f'jumppad {bx + .5} 73.5')
box(bx, bx + 2, 68, 70, 9, BRANCH)
emit(f'pad {bx + .5} 73.5 {bx + 1.5} 69.5')
add_core((bx + 1.5, 69.5))                                    # core 2
go((bx + 2.5, 74.5))
s1.gap(5); s1.run(3)
s1.sweep(5, 2.8, 0.0, 3.9, -4.9)
s1.run(2); s1.gap(3); s1.run(2)
s1.turn(DN)

# S2 (south down the east side, S6): the junction P and B2.
s2 = Run(s1.x, s1.z, DN, 6, WHITE)
s2.run(2); s2.gap(3); s2.run(2)
spot, _ = s2.wait_spot(1.4)
px = s2.x
P = add_trap(s2.lay(5, width=5), 0.0, 6)                      # P: the junction shock field
s2.advance(5)
# B2 off P's east side: 1x1 stones climb east to a perch; hop back the same way. P is crossed on
# the way in and again on the way out.
go(spot); emit(f'wtrap {P} 2.40'); go((px + 2.5, 63.5))
b2 = Run(px + 3, 63, E, 6, BRANCH)
b2.stones([(1, 0, 7), (1, -1, 8), (1, 1, 8)])
b2.gap(1)
b2.core(3, 9, 3)                                              # core 3
for p in ((px + 8.5, 64.5), (px + 6.5, 62.5), (px + 4.5, 63.5)): go(p, 'w')
emit(f'wtrap {P} 2.40'); go((px + .5, 61.3))
s2.width = 3
s2.gap(2)
s2.turn(W)
corner_checkpoint(s2)                                         # checkpoint 1

# N1 (west through the north interior, S6 -> 12 -> 10): a chain of two jump pads, a zigzag
# beam, a long jump down, hurdles and low bars.
n1 = Run(s2.x, s2.z, W, 6, SAND)
n1.run(2); n1.hurdle(); n1.run(2)
pad_a = n1.pt(-1); items.append(f'jumppad {pad_a[0]} {pad_a[1]}')
n1.gap(1)
n1.run(3, 9)                                                  # the pillar between the pads
pad_b = n1.pt(-1); items.append(f'jumppad {pad_b[0]} {pad_b[1]}')
n1.gap(2)
n1.run(2, 12)
land = n1.pt(-1.5)
go((pad_a[0] + 1.1, pad_a[1]))                               # over the hurdle first
emit(f'pad {pad_a[0]} {pad_a[1]} {pad_b[0]} {pad_b[1]}')
emit(f'pad {pad_b[0]} {pad_b[1]} {land[0]} {land[1]}')
n1.zigzag([0, 0, -2, -2, -2, 2, 2, 2, 0, 0])                  # the zigzag beam
n1.run(2, width=3)
n1.gap(6)                                                     # long jump, two blocks down
n1.run(2, 10)
n1.hurdle(); n1.run(2); n1.lowbar(); n1.run(2); n1.hurdle(); n1.run(2)
n1.core(3)                                                    # core 4
n1.gap(2); n1.run(1)
n1.turn(DN)

# N2 (south down the west side, S10): stepping stones that are shocked in turn.
n2 = Run(n1.x, n1.z, DN, 10, WHITE)
n2.run(1)
n2.shockstones([(1, -1, 10), (1, 1, 10), (1, -1, 10), (1, 1, 10)])
n2.gap(1); n2.run(1)
n2.turn(E)
corner_checkpoint(n2)                                         # checkpoint 2

# S3 (east through the middle, S10 -> 12): a pillar climb, a shocked sky beam crossed by two long
# sweepers, the fork F and B3, a sliding block, a sprint gap, a shocked crawl tunnel.
s3 = Run(n2.x, n2.z, E, 10, SAND)
s3.run(2)
s3.steps([(1, None), (2, 11), (1, None), (2, 12)])            # pillar climb
s3.run(1, 12); s3.beam(2); s3.traps(3, 3.0, width=1)
bx0 = s3.x
s3.beam(14)
Z3, ZB = s3.lane(), s3.lane() + 5                              # S3 beam and B3 beam centre lines
xa, xb = bx0 + 3.5, bx0 + 10.5                                 # the two sweepers cross here
XA = add_mover((xa, Z3 - 3.1), (xa, ZB + 3.1), 4.4, 0.0, 12)
XB = add_mover((xb, Z3 - 3.1), (xb, ZB + 3.1), 4.4, 2.2, 12)
go((xa - 2.3, Z3)); emit(f'wmover {XA} z {Z3:.2f} 2.15')
go((xb - 2.3, Z3)); emit(f'wmover {XB} z {Z3:.2f} 2.15')
fx = s3.x
s3.run(4, width=3)                                            # F
go((fx + 1.5, Z3))
# B3: a beam north off F with a shock strip, then west beside S3 (crossing both sweepers again)
# to two platforms with a core each, and back the same way.
b3 = Run(fx + 1, int(Z3) + 2, UP, 12, BRANCH, width=1)
b3.beam(1); B3T = b3.traps(2, 4.0, width=1)
b3.turn(W, size=1)
corner = b3.last
b3 = Run(b3.x, b3.z, W, 12, BRANCH, width=1)
b3.beam(2)
go((xb + 2.3, ZB)); emit(f'wmover {XB} z {ZB:.2f} 2.15')
b3.beam(4)
b3.core(3, width=3)                                           # core 5
go((xa + 2.3, ZB)); emit(f'wmover {XA} z {ZB:.2f} 2.15')
b3.beam(4, 12)
b3.core(3, width=3)                                           # core 6 (the far end)
go((xa - 2.3, ZB)); emit(f'wmover {XA} z {ZB:.2f} 2.15')
go((xa + 4, ZB))
go((xb - 2.3, ZB)); emit(f'wmover {XB} z {ZB:.2f} 2.15')
go(corner); emit(f'wtrap {B3T} 1.20'); go((fx + 1.5, Z3))
s3.run(2)
s3.slider(7, 3.0, 0.0)
s3.run(2); s3.gap(5); s3.run(2)
s3.tunnel(4, 1.0)
s3.core(3)                                                    # core 7
s3.turn(DN)

# S4 (south down the east side, S12 -> 7): the descent.
s4 = Run(s3.x, s3.z, DN, 12, WHITE)
s4.run(2)
s4.stones([(1, -1, 11), (1, 1, 10)])                          # dropping zigzag
s4.gap(2)
s4.core(3, 8)                                                 # core 8
s4.gap(1)
s4.turn(W, 7)
corner_checkpoint(s4)                                         # checkpoint 3

# S5 (west along the south, S7): a rolling shock wave, a sweeper, junction J, the main stretch or
# the B5 loop, junction K, hurdles and low bars.
s5 = Run(s4.x, s4.z, W, 7, SAND)
s5.run(2)
s5.traps(3, 0.0); s5.gap(2); s5.traps(3, 5.4); s5.gap(2); s5.traps(3, 4.8)   # a rolling wave
s5.run(2)
s5.sweep(5, 2.6, 0.0, 3.9, -4.9)
s5.gap(4); s5.run(2)
jx = s5.x
s5.run(5, width=5)                                            # J
go(s5.pt(-3))
ROUTE[0] = False                                              # the bot takes the loop instead
s5.width = 3
s5.slider(6, 2.8, 0.0); s5.run(2)                             # main stretch J -> K
K = add_trap(s5.lay(5, width=5), 3.0, 7)                      # K: the junction shock field
s5.advance(5)
ROUTE[0] = True
# B5: off J's south side, round over zigzag stones to a core, and back up into K.
b5 = Run(jx - 2, int(s5.lane()) - 3, DN, 8, BRANCH)
b5.run(2)
b5.turn(W)
b5 = Run(b5.x, b5.z, W, 8, BRANCH)
b5.stones([(1, -1, 8), (1, 1, 9), (1, -1, 9)])
b5.gap(1)
b5.core(3, 9)                                                 # core 9
b5.turn(UP)
b5 = Run(b5.x, b5.z, UP, 9, BRANCH)
b5.run(1)
go(b5.pt(-1)); emit(f'wtrap {K} 2.20')
b5.gap(1)
s5.width = 3
s5.run(2)
go(s5.pt(-1.5))
s5.gap(3); s5.run(3)
s5.hurdle(); s5.run(2); s5.lowbar(); s5.run(2); s5.hurdle(); s5.run(2)
s5.turn(DN)

# S6 (south down the west side, S7): a block sliding along the path, a sprint gap, a shock strip.
s6 = Run(s5.x, s5.z, DN, 7, WHITE)
s6.run(1)
s6.slider(6, 2.6, 0.0)
s6.run(1); s6.gap(5); s6.run(2)
s6.traps(4, 1.5)
s6.turn(E)
corner_checkpoint(s6)                                         # checkpoint 4

# S7 (east along the south edge, S7): a jump pad onto a wall top with a core, a sweeper, a flat
# six-cell sprint gap, a sliding block, a shock strip; then over the court's west wall.
s7 = Run(s6.x, s6.z, E, 7, SAND)
s7.run(2); s7.run(3)
pad_w = s7.pt(-1); items.append(f'jumppad {pad_w[0]} {pad_w[1]}')
wx = s7.x
for x in (wx, wx + 1):
    for lat in (-1, 0, 1): column(*s7.cell(x - wx, lat), 10, SLATE)
    trim(*s7.cell(x - wx, 0), 10, BEACON)
emit(f'pad {pad_w[0]} {pad_w[1]} {wx + 1.0} {pad_w[1]}')
cores.append((wx + .5, pad_w[1])); emit('core')               # core 10, on the wall top
s7.advance(2); s7.last = (wx + 1.0, pad_w[1]); s7.stand = 10
s7.run(3, 7)
go(s7.pt(-1.5))
s7.sweep(5, 2.6, 0.0, 4.6, -3.1)
s7.gap(6); s7.run(2)
s7.slider(6, 2.4, 0.0)
s7.run(2)
s7.traps(4, 2.0)
s7.gap(3); s7.run(57 - s7.x)                                # up to the court wall

# Exit court (ground level), walled off from the void.
for z in range(5, 21):
    for y in range(1, 4): cells[(57, y, z)] = SLATE
for x in range(57, 80):
    for y in range(1, 4): cells[(x, y, 20)] = SLATE
for (x, z) in ((60, 8), (60, 16), (77, 8), (77, 16)):
    for y in range(1, 6): cells[(x, y, z)] = SAND
    cells[(x, 6, z)] = BEACON
# Final bridge east over the court's west wall: step off its end into the exit court.
assert s7.x == 57, s7.x
bridge = Run(s7.x, s7.z, E, 7, SAND)
bridge.run(1)
for lat in (-1, 0, 1): trim(*bridge.cell(-1, lat), 7, BEACON)
end = bridge.pt(-1)
go((end[0] + .1, end[1])); go((end[0] + 3, end[1]))
GATE = (68, 6)
go((68.5, 9.0)); go((68.5, 7.5), 'w'); emit('exit')

# ---------------------------------------------------------------------------------------------
lines = ['# CORONA RUN:  level 3, a long timed parkour path with side branches. Generated by tools/generate_corona_run.py.',
         'name Corona Run', 'message CORONA RUN', f'room {LO} {LO} {HI} {HI}', f'height {HEIGHT}', f'wall {WALL}',
         f'time {TIME}', 'spawn 8.5 74.5 -1.5708', f'gate {GATE[0]} {GATE[1]}']
# Floor paint: the void everywhere except the exit court.
lines.append('layer 0')
for z in range(LO, HI):
    lines.append(''.join('.' if (x >= 58 and z <= 19) else VOID for x in range(LO, HI)))
for y in range(1, HEIGHT + 1):
    lines.append(f'layer {y}')
    for z in range(LO, HI):
        lines.append(''.join(cells.get((x, y, z), '.') for x in range(LO, HI)))
for (x, z) in cores: lines.append(f'core {x} {z}')
for (x0, z0, x1, z1, off, lv) in traps: lines.append(f'trap {x0} {z0} {x1} {z1} {off} {lv}')
for (a, b, p, off, lv) in movers: lines.append(f'mover {a[0]} {a[1]} {b[0]} {b[1]} {p} {off} {lv}')
lines += items
out = ROOT / 'assets/levels/corona_run.txt'
out.write_text('\n'.join(lines) + '\n', encoding='utf-8')
(ROOT / 'tests/corona_route.txt').write_text('\n'.join(route) + '\n', encoding='utf-8')
assert all(LO <= x < HI and LO <= z < HI and 1 <= y <= HEIGHT for (x, y, z) in cells), 'outside the room'
assert len(cores) == 10, len(cores)
print('Authored', out, 'voxels', len(cells), 'cores', len(cores), 'traps', len(traps), 'movers', len(movers), 'route', len(route))
