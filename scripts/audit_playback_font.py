#!/usr/bin/env python3
"""Audit a complete Tab5 library snapshot against the selected CJK playback font."""
import argparse,hashlib,json
from pathlib import Path
from fontTools.ttLib import TTFont
from playback_font_subset import EXTRA_FILE,common,read_extra,write_extra,multilingual_points,library_characters,describe
ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library',required=True,type=Path,help='Private complete library snapshot JSON')
    parser.add_argument('--report',required=True,type=Path)
    parser.add_argument('--extra-file',type=Path,default=EXTRA_FILE)
    parser.add_argument('--update-extra-codepoints',action='store_true',help='Add observed glyphs supported by the source font')
    args=parser.parse_args();raw=args.library.read_bytes();snapshot=json.loads(raw)
    for name,view in snapshot['views'].items():
        if len(view['items'])!=view['total']:raise ValueError('Incomplete library view: '+name)
        key='name' if name in ('artists','playlists') else 'title'
        if any(key not in item for item in view['items']):raise ValueError('Missing metadata in view: '+name)
    source=set(TTFont(ROOT/'assets/fonts/noto-sans-jp/NotoSansJP-Regular.otf').getBestCmap())
    base={p for p in source if common(p)};counts=library_characters(snapshot);used=set(counts)
    extra=read_extra(args.extra_file);needed=(used&source)-base
    if args.update_extra_codepoints:extra|=needed;write_extra(extra,args.extra_file)
    selected=set(multilingual_points(source,extra))
    report=dict(library_sha256=hashlib.sha256(raw).hexdigest(),audited_utc=snapshot.get('completed_utc'),
        library_revision=snapshot.get('revision'),views={name:view['total'] for name,view in snapshot['views'].items()},
        normalization='Algorithmic Hangul composition only',source_glyphs=len(source),baseline_glyphs=len(base),supplement_glyphs=len(extra-base),selected_glyphs=len(selected),
        unique_library_codepoints=len(used),covered_library_codepoints=len(used&selected),
        baseline_covered_library_codepoints=len(used&base),
        supported_supplements=describe(needed,counts),missing_from_source=describe(used-source,counts),
        missing_from_selected=describe(used-selected,counts))
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ['supported_supplements','missing_from_source','missing_from_selected']},indent=2))
    print('Extra observed glyphs:',len(needed),'Unsupported by source font:',len(used-source),'Missing from selected font:',len(used-selected))
if __name__=='__main__':main()
