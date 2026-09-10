#!/usr/bin/env python3
"""Extract original first-image format/depth provenance, without image decode."""
from pathlib import Path
import argparse, hashlib, re

def generate(repo, images):
    names=re.findall(r'^IMAGE\(([^,]+),',(repo/'assets/images.def').read_text(),re.M)
    source=(repo/'src/game/image.c').read_text()
    values=[]; digest=hashlib.sha256()
    for name in names:
        data=(images/(name+'.bin')).read_bytes()
        if len(data)<2: raise ValueError('short image '+name)
        # Exact texLoad first flag byte, then texInflateZlib's eight-bit
        # format or texInflateNonZlib's first four-bit format field.
        fmt=data[1] if data[0]&0x40 else data[1]>>4
        if fmt>12: raise ValueError('invalid image format '+name)
        values.append(fmt); digest.update(name.encode()+b'\0'+data[:2])
    out=['/* Generated original US texture header formats; '+str(len(names))+' entries. */',
         '/* Ordered source name/prefix SHA-256: '+digest.hexdigest()+' */']
    for original,native in (('g_TexFormatGbiMappings','ge_bg_texture_gbi_formats'),
                            ('g_TexFormatDepths','ge_bg_texture_gbi_depths')):
        match=re.search(r's32 '+original+r'\[\]\s*=\s*\{[^}]+\};',source)
        assert match is not None
        out.append(match[0].replace('s32 '+original,'static const int8_t '+native))
    out += ['static const uint8_t ge_bg_texture_formats[] = {']
    for offset in range(0,len(values),48):
        out.append('    '+','.join(str(x) for x in values[offset:offset+48])+',')
    out += ['};','']
    return '\n'.join(out)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('repo',type=Path);p.add_argument('images',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.write_text(generate(a.repo,a.images))
