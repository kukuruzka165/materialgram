"""Python port of codegen/emoji PrepareData list ordering (data_read.cpp + data.cpp).

Returns the emoji list in sprite order as tuples of codepoints (with FE0F kept
where the generator keeps it).
"""
import re

POSTFIX = 0xFE0F
JOINER = 0x200D
COLORS = [0x1F3FB, 0x1F3FC, 0x1F3FD, 0x1F3FE, 0x1F3FF]
COLOR_MASK = COLORS[0]
# PostfixRequired in data.cpp only affects flags, not order.


def read_file(path):
    text = open(path, encoding='utf-8').read().replace('\r\n', '\n')
    result = []
    for section in re.split(r'-{8,}|={8,}', text):
        parts = []
        for part in section.split('\n\n'):
            lines = []
            for line in part.split('\n'):
                strings = re.findall(r'"([^"]*)"', line)
                strings = [s for s in strings if s]
                if strings:
                    lines.append(strings)
            if lines:
                parts.append(lines)
        if parts:
            result.append(parts)
    return result


def ids(s):
    return tuple(ord(c) for c in s)


def bare(t):
    return tuple(c for c in t if c != POSTFIX)


def read_data(path):
    f = read_file(path)
    assert len(f) >= 3 and len(f[0]) == 8 and len(f[1]) <= 8, 'Wrong file parts'

    def colored_line(s):
        for part in f[0]:
            for line in part:
                if line[0] == s:
                    return line
        raise KeyError(s)

    colored = []
    for line in f[2][0]:
        for s in line:
            l = colored_line(s)
            assert len(l) == 6, s
            colored.append(ids(l[1]))
    double = []
    if len(f[2]) > 1:
        for line in f[2][1]:
            for s in line:
                l = colored_line(s)
                assert len(l) == 26, s
                double.append((ids(l[0]), ids(l[1]), ids(l[2])))
    categories = [[] for _ in range(8)]
    index = 0
    for part in f[0]:
        first = part[0][0]
        repl = next((p for p in f[1] if p[0][0] == first), None)
        for line in (repl or part):
            for s in line:
                categories[index].append(ids(s))
        if index + 1 < 8:
            index += 1
    other = []
    if len(f) > 3:
        for part in f[3]:
            for line in part:
                other.extend(ids(s) for s in line)
    return colored, double, categories, other


def prepare(path):
    colored, double, categories, other = read_data(path)
    aliases = {}

    def add_alias(orig, al):
        lst = aliases.setdefault(orig, [])
        if al not in lst:
            lst.append(al)

    variated = set()
    for row in colored:
        assert row[1] == COLOR_MASK
        variated.add(tuple(c for i, c in enumerate(row) if i != 1 and c != POSTFIX))

    double_variated = {}
    for original, same, different in double:
        if same.count(COLOR_MASK) == 1:
            assert same[1] == COLOR_MASK
            for color in COLORS:
                c = list(same); c[1] = color
                d = [color if e in (COLORS[0], COLORS[1]) else e for e in different]
                add_alias(bare(c), bare(d))
        else:
            if len(original) == 1:
                for color in COLORS:
                    o = bare(original + (color,))
                    s = bare(tuple(color if e == COLOR_MASK else e for e in same))
                    add_alias(o, s)
        vid = bare(original)
        same_t = (original[0], COLOR_MASK) if len(original) == 1 else same
        double_variated[vid] = (same_t, different)

    lst, mp = [], {}

    def add_one(bid, emoji):
        if bid in mp:
            assert lst[mp[bid]] == emoji or bare(lst[mp[bid]]) == bare(emoji), (bid, emoji)
            return mp[bid]
        i = len(lst)
        mp[bid] = i
        lst.append(emoji)
        for al in aliases.get(bid, []):
            assert al not in mp, 'alias already in map'
            mp[al] = i
        return i

    def append_category(cat):
        for id_ in cat:
            bid = bare(id_)
            frm, to = 0, len(id_)
            if to == 2 and id_[1] == POSTFIX:
                to -= 1
            emoji = id_[frm:to]
            assert not any(c in COLORS for c in emoji), 'color code in category'
            add_one(bid, emoji)
            if bid in variated:
                base = (id_[frm],)
                frm += 1
                rest = id_[frm:to]
                if rest.count(POSTFIX) == 2 and rest[0] == POSTFIX:
                    frm += 1
                    rest = id_[frm:to]
                for color in COLORS:
                    full = base + (color,) + rest
                    add_one(bare(full), full)
            elif bid in double_variated:
                same_t, different = double_variated[bid]
                for c1 in COLORS:
                    for c2 in COLORS:
                        pattern = same_t if c1 == c2 else different
                        out = tuple(
                            c1 if code == COLORS[0] else c2 if code == COLORS[1] else code
                            for code in pattern if code != POSTFIX)
                        add_one(out, out)

    for cat in categories:
        append_category(cat)
    append_category(other)
    return lst


if __name__ == '__main__':
    import sys
    order = prepare(sys.argv[1])
    print(len(order))
    for e in order[:3] + order[-3:]:
        print(' '.join('%x' % c for c in e))
