import math

VIEW = 240
DIAMETER_MIN = 16
DIAMETER_MAX = 48
CYCLE_MS = 5000
PULSE_COUNT = 2
REST_MS = 2000
SPIN_MS = 5400
SPIN_TURNS = 3
ORBIT_RADIUS = 40
WHITE = 0xFFDE
BLACK = 0x0000
HIGHLIGHT = (117, 116)
HIGHLIGHT_RADIUS_SQ = 6


def diameter(elapsed_ms):
    if elapsed_ms < 0:
        elapsed_ms = 0
    elapsed_ms %= CYCLE_MS
    turns = elapsed_ms / CYCLE_MS
    cosine = math.cos(turns * math.tau)
    value = int(round(32 + 16 * cosine))
    return min(DIAMETER_MAX, max(DIAMETER_MIN, value))


def smoothstep(t):
    return t * t * (3 - 2 * t)


BRIDGE_MS = CYCLE_MS // 2


def pulse_diameter(elapsed_ms):
    turns = elapsed_ms / CYCLE_MS
    cosine = math.cos(turns * math.tau)
    value = int(round(32 + 16 * cosine))
    return min(DIAMETER_MAX, max(DIAMETER_MIN, value))


def pose(elapsed_ms):
    if elapsed_ms < 0:
        elapsed_ms = 0
    pulse_span = CYCLE_MS * PULSE_COUNT
    rest_end = pulse_span + REST_MS
    shrink_end = rest_end + BRIDGE_MS
    spin_end = shrink_end + SPIN_MS
    period = spin_end + BRIDGE_MS
    elapsed_ms %= period
    if elapsed_ms < pulse_span:
        return pulse_diameter(elapsed_ms % CYCLE_MS), 0.0, 0.0
    if elapsed_ms < rest_end:
        return DIAMETER_MAX, 0.0, 0.0
    if elapsed_ms < shrink_end:
        return pulse_diameter(elapsed_ms - rest_end), 0.0, 0.0
    if elapsed_ms >= spin_end:
        return pulse_diameter(BRIDGE_MS + (elapsed_ms - spin_end)), 0.0, 0.0
    phase = (elapsed_ms - shrink_end) / SPIN_MS
    theta = phase * (SPIN_TURNS * math.tau)
    if phase < 0.5:
        radius = ORBIT_RADIUS * smoothstep(phase / 0.5)
    else:
        radius = ORBIT_RADIUS * (1 - smoothstep((phase - 0.5) / 0.5))
    return DIAMETER_MIN, radius * math.cos(theta), radius * math.sin(theta)


def inside(x, y, size, offset_x=0.0, offset_y=0.0):
    dx = 2 * ((x + 0.5) - (120.0 + offset_x))
    dy = 2 * ((y + 0.5) - (120.0 + offset_y))
    return dx * dx + dy * dy <= size * size


def scene(x, y, size, offset_x=0.0, offset_y=0.0, mirror=False):
    size = min(DIAMETER_MAX, max(DIAMETER_MIN, size))
    hx, hy = HIGHLIGHT
    if mirror:
        offset_x = -offset_x
        hx = 240 - hx
    if inside(x, y, size, offset_x, offset_y):
        if (x - (hx + offset_x)) ** 2 + (y - (hy + offset_y)) ** 2 <= HIGHLIGHT_RADIUS_SQ:
            return WHITE
        return BLACK
    return WHITE


def black_run(y, size):
    indexes = [x for x in range(VIEW) if scene(x, y, size) == BLACK]
    if not indexes:
        return 0, -1
    if indexes[-1] - indexes[0] + 1 != len(indexes):
        raise AssertionError("black pixels are not contiguous")
    return len(indexes), indexes[0]


def main():
    assert diameter(0) == DIAMETER_MAX
    assert diameter(1250) == 32
    assert diameter(CYCLE_MS // 2) == DIAMETER_MIN
    assert diameter(3750) == 32
    assert diameter(CYCLE_MS) == DIAMETER_MAX
    assert diameter(CYCLE_MS + CYCLE_MS // 2) == DIAMETER_MIN
    assert diameter(-10) == DIAMETER_MAX

    previous = diameter(0)
    for ms in range(100, CYCLE_MS // 2 + 1, 100):
        current = diameter(ms)
        assert current <= previous, (ms, previous, current)
        previous = current
    previous = diameter(CYCLE_MS // 2)
    for ms in range(CYCLE_MS // 2 + 100, CYCLE_MS + 1, 100):
        current = diameter(ms)
        assert current >= previous, (ms, previous, current)
        previous = current

    span, first = black_run(120, DIAMETER_MAX)
    assert span == DIAMETER_MAX and first == (VIEW - DIAMETER_MAX) // 2
    span, first = black_run(120, DIAMETER_MIN)
    assert span == DIAMETER_MIN and first == (VIEW - DIAMETER_MIN) // 2
    assert scene(120, 120, DIAMETER_MIN) == BLACK
    assert scene(117, 116, DIAMETER_MIN) == WHITE
    assert scene(120, 70, DIAMETER_MAX) == WHITE
    assert scene(0, 0, DIAMETER_MAX) == WHITE
    assert scene(120, 0, DIAMETER_MAX) == WHITE
    assert scene(160, 120, DIAMETER_MIN, 40, 0) == BLACK
    assert scene(120, 120, DIAMETER_MIN, 40, 0) == WHITE
    assert scene(123, 116, DIAMETER_MIN, mirror=True) == WHITE
    assert scene(117, 116, DIAMETER_MIN, mirror=True) == BLACK
    assert scene(80, 120, DIAMETER_MIN, 40, 0, True) == BLACK
    assert scene(160, 120, DIAMETER_MIN, 40, 0, True) == WHITE

    rest_start = CYCLE_MS * PULSE_COUNT
    spin_start = rest_start + REST_MS + BRIDGE_MS
    size, ox, oy = pose(7500)
    assert size == DIAMETER_MIN and ox == 0 and oy == 0
    size, ox, oy = pose(rest_start + 1000)
    assert size == DIAMETER_MAX and ox == 0 and oy == 0
    size, ox, oy = pose(spin_start)
    assert size == DIAMETER_MIN and abs(ox) < 1 and abs(oy) < 1
    _, ox, oy = pose(spin_start + SPIN_MS // 12)
    assert abs(ox) < 1 and oy > 2
    size, ox, oy = pose(spin_start + SPIN_MS // 2)
    assert size == DIAMETER_MIN and abs(ox + ORBIT_RADIUS) < 1 and abs(oy) < 1
    _, ox, oy = pose(spin_start + (SPIN_MS * 7) // 12)
    assert abs(ox) < 1 and oy < -(ORBIT_RADIUS * 0.5)
    size, _, _ = pose(spin_start + SPIN_MS - 1)
    assert size == DIAMETER_MIN
    size, ox, oy = pose(spin_start + SPIN_MS)
    assert size == DIAMETER_MIN and abs(ox) < 1 and abs(oy) < 1
    size, ox, oy = pose(spin_start + SPIN_MS + BRIDGE_MS)
    assert size == DIAMETER_MAX and abs(ox) < 1 and abs(oy) < 1
    print("pupil tests passed")


if __name__ == "__main__":
    main()
