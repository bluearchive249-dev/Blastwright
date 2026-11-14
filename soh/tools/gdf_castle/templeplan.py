# Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
"""The Temple of Light's plan: its 50 rooms, how they join, what locks each way, where the keys and treasure lie, and
a solver that proves it can be finished from any state the player can reach (no soft locks).

Three floors round one landmark, the Atrium of Light (a shaft through all three, room 49):
  F1  the Halls of Light (y 0): the Temple of Time's marble, windows and sunbeams
  B1  the Waterways (y -2400): dark rooms over glowing magical water, walkways and floating platforms
  F2  the Sanctums (y 2400): the Chamber of Sages' rounded rooms, the six Sages' colours, a starry dark
The temple has two aspects, Light and Shadow. A master mirror flips the whole temple between them:
  - F1: sun seals open in Light, moon seals in Shadow
  - B1: the magical water stands high in Light (the platforms float up to the upper doors) and low in Shadow
  - F2: bridges of light stand in Light, bridges of shadow in Shadow
The Atrium's own mirror stands on a great platform: striking it flips the temple and carries the platform (and him,
standing on it) between the Halls (Light) and the Waterways (Shadow). Mirrors linked in pairs carry him between the
floors, some only in one aspect. Floors are otherwise not joined: the way through is back and forth between aspects.

Requirements on a way through (each side of a door):
  None   open         'L' / 'S'  in Light / Shadow only
  'key'  a small key (spent once; the door stays open)   'boss' the boss key
  'puz'  the puzzle in the room on the near side solved   'clr'  that room's guards beaten
"""
import collections, itertools

FLOORS = {'F1': 0, 'B1': -2400, 'F2': 2400}

# id: (name, floor, (col, row), kind)
ROOMS = {
    0: ('Entrance Hall', 'F1', (2, 3), 'plates'),
    1: ('Sanctum of Light', 'F1', (2, 2), 'hub'),
    2: ('Hall of Spears', 'F1', (1, 2), 'combat'),
    3: ('Brazier Gallery', 'F1', (3, 2), 'braziers'),
    4: ('West Cloister', 'F1', (0, 2), 'braziers'),
    5: ('Hall of Four Flames', 'F1', (4, 2), 'braziers'),
    6: ('Lantern Room', 'F1', (0, 3), 'treasure'),
    7: ('Hall of Triangles', 'F1', (1, 3), 'plates'),
    8: ('Crystal Hall', 'F1', (3, 3), 'crystals'),
    9: ('Shrine of Dusk', 'F1', (4, 3), 'mirror'),
    10: ('Moon Court', 'F1', (0, 1), 'quiet'),
    11: ('Pilgrims\' Walk', 'F1', (1, 1), 'quiet'),
    12: ('Hall of the Sun Mirror', 'F1', (3, 1), 'beam'),
    13: ('Block Vault', 'F1', (4, 1), 'block'),
    14: ('Gallery of Dusk', 'F1', (0, 0), 'quiet'),
    15: ('Chasm of Pillars', 'F1', (1, 0), 'chasm'),
    16: ('North Hall', 'F1', (2, 0), 'quiet'),
    17: ('Timed Bridge', 'F1', (3, 0), 'timed'),
    18: ('Mirror Landing', 'F1', (4, 0), 'quiet'),
    19: ('Sunken Archive', 'B1', (0, 0), 'quiet'),
    20: ('Waterwheel Hall', 'B1', (1, 0), 'quiet'),
    21: ('North Channel', 'B1', (2, 0), 'water'),
    22: ('Broken Aqueduct', 'B1', (3, 0), 'chasm'),
    23: ('Reflecting Pool', 'B1', (4, 0), 'water'),
    24: ('Drowned Crypt', 'B1', (0, 1), 'water'),
    25: ('Captain\'s Moat', 'B1', (1, 1), 'miniboss'),
    26: ('Weir of Light', 'B1', (3, 1), 'beam'),
    27: ('Mirror of the Deep', 'B1', (4, 1), 'mirror'),
    28: ('Still Water', 'B1', (0, 2), 'treasure'),
    29: ('Spillway', 'B1', (1, 2), 'water'),
    30: ('Dark Cistern', 'B1', (2, 2), 'dark'),
    31: ('Floodgate', 'B1', (3, 2), 'water'),
    32: ('Undercroft', 'B1', (4, 2), 'treasure'),
    33: ('Hidden Font', 'B1', (2, 3), 'secret'),
    34: ('Sanctum of Forest', 'F2', (0, 0), 'quiet'),
    35: ('Sanctum of Water', 'F2', (1, 0), 'bridges'),
    36: ('Hall of the Six', 'F2', (2, 0), 'quiet'),
    37: ('Sanctum of Fire', 'F2', (3, 0), 'simon'),
    38: ('Sanctum of Shadow', 'F2', (4, 0), 'bridges'),
    39: ('Sanctum of Spirit', 'F2', (0, 1), 'crystals'),
    40: ('Mirror of the Heavens', 'F2', (1, 1), 'mirror'),
    41: ('Star Bridge', 'F2', (3, 1), 'bridges'),
    42: ('Sky Chasm', 'F2', (4, 1), 'chasm'),
    43: ('Sanctum of Light', 'F2', (0, 2), 'quiet'),
    44: ('Knights\' Rest', 'F2', (1, 2), 'combat'),
    45: ('Celestial Walk', 'F2', (2, 2), 'bridges'),
    46: ('Prism Hall', 'F2', (3, 2), 'beam'),
    47: ('Vault of the Sages', 'F2', (4, 2), 'treasure'),
    48: ('Antechamber', 'F2', (2, -1), 'ante'),
    49: ('Atrium of Light', 'ALL', (2, 1), 'atrium'),
}
ATRIUM = 49
# The atrium's places (logical nodes): its Halls balcony, Waterways dock, Sanctums gallery
A1, A0, A2 = 'A1', 'A0', 'A2'
ATRIUM_NODE = {'F1': A1, 'B1': A0, 'F2': A2}

# Doors: (room a, room b, requirement from a's side, requirement from b's side). Sides: b is the neighbour of a in
# the grid (or the atrium). 'puz'/'clr' refer to the room whose puzzle or guards open the door (written as such).
DOORS = [
    (0, 1, ('puz', 0), ('puz', 0)),          # the great door: the plates (teach)
    (0, 8, None, None),
    (0, 7, ('puz', 7), ('puz', 7)),          # a shortcut back: opened from the Hall of Triangles' side
    (1, 2, None, None),
    (1, 3, None, None),
    (1, ATRIUM, 'S', 'S'),                   # moon seal: the Atrium is seen through it at once, reached later
    (2, 4, ('clr', 2), ('clr', 2)),
    (2, 7, ('clr', 2), ('clr', 2)),
    (3, 5, 'key', 'key'),
    (3, 8, None, None),
    (3, 12, 'S', 'S'),                       # moon seal, seen at once, opened much later
    (4, 6, None, None),
    (4, 10, 'S', 'S'),                       # moon seal
    (6, 7, None, None),
    (8, 9, ('puz', 8), ('puz', 8)),
    (5, 9, None, None),
    (5, 13, ('puz', 5), ('puz', 5)),
    (10, 11, 'S', 'S'),
    (10, 14, None, None),
    (11, 15, 'key', 'key'),
    (11, ATRIUM, None, None),
    (12, ATRIUM, None, None),
    (12, 13, 'key', 'key'),
    (12, 17, ('puz', 12), ('puz', 12)),
    (13, 18, None, None),
    (14, 15, 'L', 'L'),
    (15, 16, None, None),
    (16, ATRIUM, 'S', 'S'),
    (16, 17, 'key', 'key'),
    (17, 18, None, None),
    # the Waterways (L: the water high, the upper doors reached; S: the water low, the lower doors)
    (ATRIUM, 25, None, None),
    (ATRIUM, 26, 'S', 'S'),
    (ATRIUM, 21, 'L', 'L'),
    (ATRIUM, 30, None, None),
    (19, 20, None, None),
    (19, 24, 'S', 'S'),
    (20, 21, 'key', 'key'),
    (20, 25, 'L', 'L'),
    (21, 22, None, None),
    (22, 23, 'S', 'S'),
    (22, 26, 'L', 'L'),
    (23, 27, None, None),
    (24, 25, 'L', 'L'),
    (24, 28, None, None),
    (25, 29, ('clr', 25), ('clr', 25)),     # the Guard Captain
    (26, 27, ('puz', 26), ('puz', 26)),
    (26, 31, None, None),
    (27, 32, 'key', 'key'),
    (28, 29, 'S', 'S'),
    (29, 30, None, None),
    (30, 31, 'L', 'L'),
    (30, 33, ('puz', 30), ('puz', 30)),     # the hidden font: light the dark cistern's braziers
    (31, 32, None, None),
    # the Sanctums (L: bridges of light; S: bridges of shadow)
    (34, 35, 'L', 'L'),
    (34, 39, None, None),
    (35, 36, 'key', 'key'),
    (35, 40, 'S', 'S'),
    (36, ATRIUM, 'L', 'L'),
    (36, 37, None, None),
    (36, 48, 'boss', 'boss'),
    (37, 38, 'L', 'L'),
    (37, 41, ('puz', 37), ('puz', 37)),
    (38, 42, None, None),
    (39, 40, ('puz', 39), ('puz', 39)),
    (39, 43, 'L', 'L'),
    (40, ATRIUM, None, None),
    (40, 44, None, None),
    (41, ATRIUM, None, None),
    (41, 42, 'key', 'key'),
    (41, 46, 'S', 'S'),
    (42, 47, 'S', 'S'),
    (43, 44, None, None),
    (44, 45, ('clr', 44), ('clr', 44)),
    (45, ATRIUM, 'L', 'L'),
    (45, 46, None, None),
]

# Mirrors linking floors: (room a, room b, aspect they work in or None)
TELEPORTS = [
    (18, 23, 'S'),
    (9, 31, 'S'),
    (19, 43, 'L'),
    (33, 6, 'oneway'),  # the hidden font's mirror: a way back up to the Halls (its far end is only a reflection)
]
# Master mirrors (flip the temple's aspect where they stand)
MASTERS = [9, 27, 40]
# The atrium's own: (A1, Light) <-> (A0, Shadow), riding its platform

# Chests: room -> list of items. 'key' small key, 'boss' boss key, else treasure.
CHESTS = {
    3: ['key'],           # the braziers' reward (teach)
    7: ['key'],           # the plates' reward (test)
    6: ['rupee50'],
    4: ['rupee20'],       # the West Cloister's braziers
    13: ['key'],          # the block vault
    9: ['magic'],
    10: ['heart'],
    17: ['key'],
    22: ['key'],
    24: ['rupee50'],
    28: ['key'],
    32: ['rupee200'],
    33: ['heartpiece'],
    38: ['key'],
    39: ['magic'],
    43: ['key'],
    44: ['rupee50'],
    47: ['boss'],
    46: ['key'],
    25: ['heart'],
    15: ['rupee20'],
    42: ['rupee20'],
}
START = 0
GOAL = 48


def node_of(room, floor=None):
    if room == ATRIUM:
        return ATRIUM_NODE[floor]
    return room


def floor_of(room):
    return ROOMS[room][1]


def build_graph():
    """Edges between logical places: (u, v, requirement) both ways (a's requirement crossing from a, b's from b)."""
    edges = []
    for a, b, ra, rb in DOORS:
        fa = floor_of(a) if a != ATRIUM else floor_of(b)
        fb = floor_of(b) if b != ATRIUM else floor_of(a)
        u, v = node_of(a, fb), node_of(b, fa)
        edges.append((u, v, ra, ('door', a, b)))
        edges.append((v, u, rb, ('door', a, b)))
    for a, b, st in TELEPORTS:
        if st == 'oneway':
            edges.append((a, b, None, ('mirror', a, b)))
            continue
        edges.append((a, b, st, ('mirror', a, b)))
        edges.append((b, a, st, ('mirror', a, b)))
    return edges


EDGES = build_graph()
KEY_DOORS = sorted({e[3] for e in EDGES if e[2] == 'key'})
CHEST_LIST = [(r, i, item) for r, items in sorted(CHESTS.items()) for i, item in enumerate(items)]


def can_cross(req, aspect, solved, opened, door, keys_left, has_boss):
    if req is None:
        return True
    if req in ('L', 'S'):
        return req == aspect
    if req == 'key':
        return door in opened
    if req == 'boss':
        return has_boss
    return req[1] in solved   # puzzle or guards of that room


def reach(start, aspect, solved, opened, has_boss):
    """Places reachable from start in this aspect (puzzles of reached rooms count as solved as he goes)."""
    seen = {start}
    stack = [start]
    solved = set(solved)
    changed = True
    while changed:
        changed = False
        stack = list(seen)
        while stack:
            u = stack.pop()
            room = ATRIUM if isinstance(u, str) else u
            if ROOMS[room][3] in ('plates', 'braziers', 'crystals', 'beam', 'block', 'simon', 'combat', 'miniboss',
                                  'dark') and room not in solved:
                solved.add(room)
                changed = True
            for a, b, req, door in EDGES:
                if a == u and b not in seen and can_cross(req, aspect, solved, opened, door, 0, has_boss):
                    seen.add(b)
                    stack.append(b)
    return seen, frozenset(solved)


def master_here(node, aspect):
    """Where striking a master mirror at this place leaves him: (place, new aspect), or None."""
    room = ATRIUM if isinstance(node, str) else node
    if room in MASTERS:
        return (node, 'S' if aspect == 'L' else 'L')
    if node == A1 and aspect == 'L':
        return (A0, 'S')
    if node == A0 and aspect == 'S':
        return (A1, 'L')
    return None


LOGIC_ITEMS = ('key', 'boss')


def settle(node, aspect, chests, doors, solved):
    """Opens every key and boss chest he can reach (never a loss: so taken at once) and returns the settled state with
    its region's representative place."""
    while True:
        has_boss = any(CHEST_LIST[i][2] == 'boss' for i in chests)
        places, solved = reach(node, aspect, solved, doors, has_boss)
        more = {i for i, (r, _, item) in enumerate(CHEST_LIST)
                if item in LOGIC_ITEMS and i not in chests and r in places}
        if not more:
            # (one place stands for its whole region; with the one-way mirror a region reached from one place may be
            # bigger than from another, so the region's representative is the place's set of mutually reachable
            # places' smallest member)
            back = {q for q in places if node in reach(q, aspect, solved, doors, has_boss)[0]}
            rep = min(back, key=lambda p: (isinstance(p, str), str(p)))
            return (rep, aspect, frozenset(chests), frozenset(doors), solved), places
        chests = frozenset(chests) | more


def moves(state):
    node, aspect, chests, doors, solved = state
    st, places = settle(node, aspect, chests, doors, solved)
    node, aspect, chests, doors, solved = st
    keys_held = sum(1 for i in chests if CHEST_LIST[i][2] == 'key') - len(doors)
    out = []
    if keys_held > 0:
        for a, b, req, door in EDGES:
            if req == 'key' and door not in doors and a in places:
                out.append(('key', door, settle(a, aspect, chests, doors | {door}, solved)[0]))
    for p in places:
        m = master_here(p, aspect)
        if m:
            out.append(('mirror', p, settle(m[0], m[1], chests, doors, solved)[0]))
    return st, places, out


def solve(verbose=False):
    """Explores every state the player can get into (where, aspect, key doors opened: chests that matter are taken at
    once). From each the boss's door must still be reachable: no soft locks, whatever order keys are spent in."""
    start, _ = settle(START, 'L', frozenset(), frozenset(), frozenset())
    K = lambda st: st[:4]
    seen = {K(start): start}
    order = [start]
    graph = collections.defaultdict(set)
    goal = set()
    rooms_reached = set()
    chests_reached = set()
    i = 0
    while i < len(order):
        st = order[i]
        i += 1
        st, places, out = moves(st)
        rooms_reached |= {ATRIUM if isinstance(p, str) else p for p in places}
        chests_reached |= {j for j, (r, _, item) in enumerate(CHEST_LIST) if r in places}
        if GOAL in places:
            goal.add(K(st))
        for kind, what, n in out:
            graph[K(st)].add(K(n))
            if K(n) not in seen:
                seen[K(n)] = n
                order.append(n)
    rev = collections.defaultdict(set)
    for u, vs in graph.items():
        for v in vs:
            rev[v].add(u)
    good = set(goal)
    stack = list(goal)
    while stack:
        v = stack.pop()
        for u in rev[v]:
            if u not in good:
                good.add(u)
                stack.append(u)
    bad = [k for k in seen if k not in good]
    report = dict(states=len(seen), goal_states=len(goal), dead_ends=len(bad),
                  unreachable_chests=[CHEST_LIST[j] for j in range(len(CHEST_LIST)) if j not in chests_reached],
                  missing_rooms=sorted(set(ROOMS) - rooms_reached),
                  keys=sum(1 for c in CHEST_LIST if c[2] == 'key'), key_doors=len(KEY_DOORS))
    if bad and verbose:
        report['dead_end_example'] = [(k[0], k[1], sorted(CHEST_LIST[j][0] for j in k[2]), sorted(k[3])) for k in bad[:3]]
    ok = (not bad) and (not report['unreachable_chests']) and (not report['missing_rooms']) and bool(goal)
    return ok, report


def shortest_route():
    """The route to the boss's door with the fewest mirror strikes (the back and forth): the strikes and keys, in
    order."""
    import heapq
    start, _ = settle(START, 'L', frozenset(), frozenset(), frozenset())
    best = set()
    pq = [(0, 0, start, ())]
    n = 0
    while pq:
        cost, _, st, path = heapq.heappop(pq)
        k = st[:4]
        if k in best:
            continue
        best.add(k)
        st, places, out = moves(st)
        if GOAL in places:
            return cost, path
        for kind, what, nst in out:
            n += 1
            heapq.heappush(pq, (cost + (1 if kind == 'mirror' else 0), n, nst, path + ((kind, what, nst[1]),)))
    return None, ()


if __name__ == '__main__':
    ok, rep = solve(verbose=True)
    print('OK' if ok else 'FAILED', rep)
    c, path = shortest_route()
    print('mirror strikes on the shortest way to the boss:', c)
    for step in path:
        print('  ', step)
