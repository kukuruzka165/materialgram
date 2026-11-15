"""Build the bundled Noto 3D emoji set (Resources/emoji/noto3d/emoji_N.webp) from
Google's Noto 3D emoji (CC BY 4.0, https://googlefonts.github.io/noto-emoji-files/).

Usage: python Telegram/build/emoji_noto3d.py <download-cache-dir>

Sprite order comes from lib_ui/emoji.txt via emoji_order.py, a port of the
codegen_emoji ordering. Rerun after updating emoji.txt.
"""
import os
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor

from PIL import Image

import emoji_order

URL = 'https://fonts.gstatic.com/s/e/noto3demoji/latest/{}/128.png'
SIZE, PER_ROW, ROWS = 72, 32, 16  # must match lib_ui/ui/emoji_config.cpp
PREVIEW = ['1f603', '1f609', '1f614', '1f628']  # 😃 😉 😔 😨

here = os.path.dirname(os.path.abspath(__file__))
telegram = os.path.dirname(here)
emoji = os.path.join(telegram, 'Resources', 'emoji')
out = os.path.join(emoji, 'noto3d')


def name(emoji):
    return '_'.join('%04x' % c for c in emoji if c != 0xFE0F)


def fetch(cache, key):
    path = os.path.join(cache, key + '.png')
    if not os.path.exists(path):
        with urllib.request.urlopen(URL.format(key), timeout=60) as r:
            data = r.read()
        with open(path + '.tmp', 'wb') as f:
            f.write(data)
        os.replace(path + '.tmp', path)
    return path


def cell(path):
    return Image.open(path).convert('RGBA').resize((SIZE, SIZE), Image.LANCZOS)


def main(cache):
    os.makedirs(cache, exist_ok=True)
    os.makedirs(out, exist_ok=True)
    order = emoji_order.prepare(os.path.join(telegram, 'lib_ui', 'emoji.txt'))
    keys = [name(e) for e in order]
    with ThreadPoolExecutor(16) as pool:
        paths = list(pool.map(lambda k: fetch(cache, k), keys))

    per_sheet = PER_ROW * ROWS
    for sheet in range(0, len(paths), per_sheet):
        chunk = paths[sheet:sheet + per_sheet]
        rows = -(-len(chunk) // PER_ROW)
        image = Image.new('RGBA', (PER_ROW * SIZE, rows * SIZE))
        for i, path in enumerate(chunk):
            image.paste(cell(path), ((i % PER_ROW) * SIZE, (i // PER_ROW) * SIZE))
        target = os.path.join(out, 'emoji_%d.webp' % (sheet // per_sheet + 1))
        image.save(target, 'WEBP', quality=99, method=6)
        print(target)

    preview = Image.new('RGBA', (SIZE * len(PREVIEW), SIZE))
    for i, key in enumerate(PREVIEW):
        preview.paste(cell(fetch(cache, key)), (i * SIZE, 0))
    preview.save(os.path.join(emoji, 'set4_preview.webp'), 'WEBP', quality=99, method=6)
    print('%d emoji done' % len(paths))


if __name__ == '__main__':
    main(sys.argv[1])
