"""Regenerate docs/progress.svg and the progress block in README.md from docs/progress.json.

usage:
  python -I tools/update_progress.py                      # just regenerate
  python -I tools/update_progress.py --note "what changed" [--set "<stage name prefix>=<percent>" ...]

Run it after every iteration (then commit and push). Only the text between
<!-- progress:start --> and <!-- progress:end --> in README.md is rewritten.
"""
import argparse, datetime, html, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
JSON = ROOT / 'docs' / 'progress.json'
SVG = ROOT / 'docs' / 'progress.svg'
README = ROOT / 'README.md'
START, END = '<!-- progress:start -->', '<!-- progress:end -->'


def overall(stages):
    tw = sum(s['weight'] for s in stages)
    return sum(s['weight'] * s['done'] for s in stages) / tw


def text_bar(pct, width=20):
    n = round(pct / 100 * width)
    return '█' * n + '░' * (width - n)


def make_svg(pct):
    w, h, pad = 560, 34, 2
    inner = w - 2 * pad
    fill = max(0.0, min(100.0, pct)) / 100 * inner
    label = f'{round(pct)}%'
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" role="img" aria-label="Project progress {label}">
  <title>Project progress {label}</title>
  <defs>
    <linearGradient id="g" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#3fb950"/>
      <stop offset="1" stop-color="#2ea043"/>
    </linearGradient>
    <clipPath id="c"><rect x="{pad}" y="{pad}" width="{inner}" height="{h - 2 * pad}" rx="8"/></clipPath>
  </defs>
  <rect x="0.5" y="0.5" width="{w - 1}" height="{h - 1}" rx="9" fill="#d0d7de" stroke="#8c959f"/>
  <g clip-path="url(#c)">
    <rect x="{pad}" y="{pad}" width="{fill:.1f}" height="{h - 2 * pad}" fill="url(#g)"/>
  </g>
  <text x="{w / 2}" y="{h / 2 + 5}" text-anchor="middle" font-family="Segoe UI, Helvetica, Arial, sans-serif" font-size="15" font-weight="700" fill="#ffffff" stroke="#1a7f37" stroke-width="0.6" paint-order="stroke">{label}</text>
</svg>
'''


def make_block(data, pct):
    rows = ['| Stage | Weight | Done |', '|---|---:|---|']
    for s in data['stages']:
        rows.append(f"| {s['name']} | {s['weight']}% | `{text_bar(s['done'], 10)}` {s['done']}% |")
    lines = [
        START,
        '## Progress',
        '',
        f'![Project progress {round(pct)}%](docs/progress.svg)',
        '',
        f'**Overall: about {round(pct)}%** (effort-weighted estimate; the real risk is not evenly spread).',
        '',
        *rows,
        '',
        f"_Last update: {data['updated']}. {html.escape(data['note'], quote=False)}_",
        END,
    ]
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--note')
    ap.add_argument('--set', action='append', default=[], help='"<stage name prefix>=<percent>"')
    a = ap.parse_args()
    data = json.loads(JSON.read_text(encoding='utf-8'))
    for item in a.set:
        key, _, val = item.rpartition('=')
        hits = [s for s in data['stages'] if s['name'].lower().startswith(key.strip().lower())]
        if len(hits) != 1:
            sys.exit(f'--set {item!r}: {len(hits)} stages match')
        hits[0]['done'] = max(0, min(100, float(val)))
    if a.note:
        data['note'] = a.note
    if a.note or a.set:
        data['updated'] = datetime.date.today().isoformat()
        JSON.write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    pct = overall(data['stages'])
    SVG.write_text(make_svg(pct), encoding='utf-8')
    block = make_block(data, pct)
    txt = README.read_text(encoding='utf-8')
    if START in txt and END in txt:
        txt = re.sub(re.escape(START) + r'.*?' + re.escape(END), lambda m: block, txt, flags=re.S)
    else:
        txt = txt.rstrip('\n') + '\n\n' + block + '\n'
    README.write_text(txt, encoding='utf-8', newline='\n')
    print(f'overall {pct:.1f}%  -> {SVG.name}, README block updated')


main()
