#!/usr/bin/env python3
"""Brings a directory of place files to schema 3, which is the only one AIS-catcher reads.

    python3 scripts/migrate-places.py <places directory> [--write]

Without --write nothing is changed; the report says what would be. Files already at
schema 3 are left alone, and a converted file keeps the formatting it had. A
FeatureCollection is converted feature by feature.

Schema 1 (edge builds before Sep 23, 2026): the code moves under codes, the port a
berth or anchorage named becomes part_of when that port is in the same directory,
details are split into attributes and geometry_source, custom becomes guardzone.
Schema 2: area becomes guardzone, attributes.area_subtype becomes category. In
both, size becomes rank. The revision goes up by one, as with any edit.
"""
import argparse
import json
import re
import sys
from pathlib import Path

LOCODE = re.compile(r'[A-Z]{2}[A-Z0-9]{3}')
INVENTED = re.compile(r'[A-Z]{2}0[A-Z0-9]{2}')  # the hub's own port codes, never in the register


# how the file was written, so that converting it changes nothing else
STYLES = ({'indent': 1}, {'separators': (',', ':')}, {'indent': 2})


def style(text, document):
    for kwargs in STYLES:
        if json.dumps(document, ensure_ascii=False, **kwargs) == text.strip():
            return kwargs
    return STYLES[0]


def features(document):
    return document['features'] if document.get('type') == 'FeatureCollection' else [document]


def upgrade(feature, ports, notes):
    p = feature['properties']
    version = p.get('schema_version')
    if version == 3:
        return False
    if version not in (1, 2):
        raise ValueError('schema_version %r' % version)
    name = p.get('name', feature.get('id'))
    if version == 1:
        details = p.pop('details', None) or {}
        codes, attributes, source = {}, {}, {}
        code = p.pop('unlocode', None)
        if code:
            if INVENTED.fullmatch(code) or not LOCODE.fullmatch(code):
                codes['own'] = ['local:' + code]
            else:
                codes['unlocode'] = [code]
        own = details.pop('code', None)
        if own:
            codes.setdefault('own', []).append('local:' + own)
        parent = p.pop('parent_unlocode', None) or p.pop('port_unlocode', None)
        p.pop('port_unlocode', None)
        terminal = details.pop('terminal', None)
        if terminal:
            p['part_of'] = terminal
        elif parent:
            if parent in ports:
                p['part_of'] = ports[parent]
            else:
                notes.append('%s: names port %s, which is not in this directory' % (name, parent))
        category = p.pop('category', None)
        if p.get('place_type') == 'custom':
            p['place_type'] = 'guardzone'
        if category:
            p['category'] = str(category).lower()
        for key in ('source', 'url', 'osm', 'confidence'):
            if key in details:
                source[key] = details.pop(key)
        attributes.update(details)
        if codes:
            p['codes'] = codes
        if attributes:
            p['attributes'] = attributes
        if source:
            p['geometry_source'] = source
    else:
        if p.get('place_type') == 'area':
            p['place_type'] = 'guardzone'
        attributes = p.get('attributes')
        if isinstance(attributes, dict) and 'area_subtype' in attributes:
            p['category'] = attributes.pop('area_subtype')
            if not attributes:
                del p['attributes']
    if 'size' in p:  # renamed where it stands
        renamed = {('rank' if key == 'size' else key): value for key, value in p.items()}
        p.clear()
        p.update(renamed)
    p['schema_version'] = 3
    p['revision'] = max(1, int(p.get('revision', 0)) + 1)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--write', action='store_true', help='rewrite the files; without it, only report')
    args = parser.parse_args()
    files = sorted(args.directory.glob('*.geojson'))
    documents, styles = {}, {}
    for path in files:
        try:
            text = path.read_text(encoding='utf-8')
            documents[path] = json.loads(text)
            styles[path] = style(text, documents[path])
        except (OSError, ValueError) as e:
            print('skipped %s: %s' % (path.name, e), file=sys.stderr)
    # the ports of this directory by the code they designate, for schema 1 links
    ports = {}
    for document in documents.values():
        for f in features(document):
            p = f.get('properties', {})
            if p.get('place_type') != 'port':
                continue
            for code in [p['unlocode']] if p.get('unlocode') else p.get('codes', {}).get('unlocode', []):
                ports.setdefault(code, f['id'])
    converted, notes = 0, []
    for path, document in documents.items():
        changed = False
        for f in features(document):
            try:
                changed |= upgrade(f, ports, notes)
            except (KeyError, ValueError, TypeError) as e:
                notes.append('%s: not converted (%s)' % (path.name, e))
        if not changed:
            continue
        converted += 1
        if args.write:
            temp = path.with_suffix('.tmp')
            temp.write_text(json.dumps(document, ensure_ascii=False, **styles[path]) + '\n', encoding='utf-8')
            temp.replace(path)
    for note in notes:
        print(note)
    print('%d of %d files %s' % (converted, len(documents), 'converted' if args.write else 'would be converted; run again with --write'))


if __name__ == '__main__':
    main()
