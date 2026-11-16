"""Deterministic Japanese, frequency-ranked Chinese and common Korean repertoire."""
from collections import Counter
from pathlib import Path
import unicodedata

EXTRA_FILE=Path(__file__).resolve().parents[1]/'assets/fonts/noto-sans-jp/extra-codepoints.txt'
CHINESE_FILE=EXTRA_FILE.parent/'chinese-top1500.txt'

def valid_point(point):
    return 0<=point<=0x10ffff and not 0xd800<=point<=0xdfff

def common(point):
    if not valid_point(point):return False
    if point<0x100 or 0x2000<=point<=0x206f or 0x3000<=point<=0x30ff or 0xff00<=point<=0xffef:return True
    try:chr(point).encode('cp932');return True
    except UnicodeEncodeError:return False

def read_extra(path=EXTRA_FILE):
    points=set()
    if not path.exists():return points
    for number,line in enumerate(path.read_text().splitlines(),1):
        token=line.split('#',1)[0].strip()
        if not token:continue
        if not token.startswith('U+'):raise ValueError(f'{path}:{number}: expected U+XXXX')
        try:point=int(token[2:],16)
        except ValueError:raise ValueError(f'{path}:{number}: invalid codepoint') from None
        if not valid_point(point):raise ValueError(f'{path}:{number}: invalid Unicode scalar')
        points.add(point)
    return points

def write_extra(points,path=EXTRA_FILE):
    lines=['# Extra glyphs observed in the audited music library, beyond the common baseline.',
           '# Unicode scalars only; no track/artist metadata is stored here.']
    for point in sorted(points):lines.append(f'U+{point:04X} # {unicodedata.name(chr(point),"UNNAMED")}')
    path.write_text('\n'.join(lines)+'\n')

def select(source,extra):
    source=set(source);extra=set(extra)
    if extra-source:raise ValueError('Requested supplements are absent from the source font: '+', '.join(f'U+{p:04X}' for p in sorted(extra-source)))
    return sorted({point for point in source if common(point)}|extra)

def korean_points():
    # KS X 1001 Hangul block only, excluding its Hanja rows.
    syllables={ord(bytes([lead,trail]).decode('euc_kr')) for lead in range(0xb0,0xc9) for trail in range(0xa1,0xff)}
    jamo=set(range(0x1100,0x1113))|set(range(0x1161,0x1176))|set(range(0x11a8,0x11c3))|set(range(0x3131,0x318f))
    return syllables|jamo

def multilingual_points(source,extra):
    return sorted(set(select(source,extra))|read_extra(CHINESE_FILE)|korean_points())

def glyph_source(point,cmaps):
    # Shared Unicode Han uses one Japanese glyph, even where regional forms differ.
    if point in cmaps['jp']:return 'jp'
    region='kr' if 0x1100<=point<=0x11ff or 0x3130<=point<=0x318f or 0xac00<=point<=0xd7a3 else 'sc'
    if point not in cmaps[region]:raise ValueError(f'U+{point:04X} is absent from {region} font')
    return region

def compose_hangul(text):
    # Unicode algorithmic Hangul composition, without normalizing other scripts.
    result=[]
    for ch in text:
        point=ord(ch)
        if result:
            last=ord(result[-1])
            if 0x1100<=last<0x1113 and 0x1161<=point<0x1176:
                result[-1]=chr(0xac00+((last-0x1100)*21+point-0x1161)*28);continue
            if 0xac00<=last<=0xd7a3 and (last-0xac00)%28==0 and 0x11a8<=point<0x11c3:
                result[-1]=chr(last+point-0x11a7);continue
        result.append(ch)
    return ''.join(result)

def library_characters(snapshot):
    counts=Counter()
    for view in snapshot['views'].values():
        for item in view['items']:
            for key in ('title','artist','album','name'):
                text=item.get(key)
                if isinstance(text,str):counts.update(map(ord,compose_hangul(text)))
    return counts

def describe(points,counts):
    return [dict(codepoint=f'U+{point:04X}',character=chr(point),name=unicodedata.name(chr(point),'UNNAMED'),occurrences=counts[point]) for point in sorted(points)]
