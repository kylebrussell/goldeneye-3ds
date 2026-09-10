#!/usr/bin/env python3
"""Extract verified original image/LOD metadata for native tex.c emission."""
from pathlib import Path
import argparse, hashlib, json, re

def generate(repo, images, catalog):
    names=re.findall(r'^IMAGE\(([^,]+),',(repo/'assets/images.def').read_text(),re.M)
    source=(repo/'src/game/image.c').read_text()
    records={record['source']:record for record in json.loads(catalog.read_text())['textures']}
    metadata=[]; digest=hashlib.sha256()
    for name in names:
        data=(images/(name+'.bin')).read_bytes()
        if len(data)<2: raise ValueError('short image '+name)
        # Exact texLoad first flag byte, then texInflateZlib's eight-bit
        # format or texInflateNonZlib's first four-bit format field.
        fmt=data[1] if data[0]&0x40 else data[1]>>4
        if fmt>12: raise ValueError('invalid image format '+name)
        digest.update(name.encode()+b'\0'+data[:2])
        record=records[name+'.bin']
        if hashlib.sha256(data).hexdigest()!=record['source_sha256']:
            raise ValueError('texture catalog/source mismatch '+name)
        lods=sorted(record['lods'],key=lambda x:x['lod'])
        if [x['lod'] for x in lods] != list(range(len(lods))): raise ValueError('noncontiguous LODs '+name)
        maxlod=data[0]&0x3f; explicit=(data[0]>>7)&1
        if maxlod>7 or len(lods)>7: raise ValueError('too many LODs '+name)
        if explicit and len(lods)!=max(maxlod,1): raise ValueError('missing explicit LOD '+name)
        widths=[x['width'] for x in lods]; heights=[x['height'] for x in lods]
        if not all(1<=n<=255 for n in widths+heights): raise ValueError('invalid dimensions '+name)
        if data[0]&0x40:
            base=3+(data[2]+1)*2
            source_width,source_height=data[base:base+2]
        else:
            source_width=((data[1]&15)<<4)|(data[2]>>4)
            source_height=((data[2]&15)<<4)|(data[3]>>4)
        if (widths[0],heights[0])!=(source_width,source_height):
            raise ValueError('catalog/base image dimensions mismatch '+name)
        # texInflateZlib's implicit palette mip builder stops before its
        # original 0x800-byte output ceiling. Pixel values do not affect size.
        palette=data[2] if data[0]&0x40 else 0
        if data[0]&0x40 and not explicit and maxlod>=2:
            if fmt not in (9,10,11,12): raise ValueError('nonpaletted zlib '+name)
            div=1 if fmt in (9,11) else 2
            width,height=widths[0],heights[0]
            size=((width+div*8-1)//(div*8))*8*height
            for lod in range(1,maxlod):
                width=(width+1)//2; height=(height+1)//2
                extra=((width+div*8-1)//(div*8))*8*height
                if size+extra>0x800: maxlod=lod; break
                size+=extra
        metadata.append((fmt,maxlod,explicit,palette,widths+[0]*(8-len(widths)),heights+[0]*(8-len(heights))))
        digest.update(bytes([maxlod,explicit,palette]+metadata[-1][4]+metadata[-1][5]))
    out=['/* Generated original US texture/LOD metadata; '+str(len(names))+' entries. */',
         '/* Ordered name/header/LOD metadata SHA-256: '+digest.hexdigest()+' */']
    for original,native in (('g_TexFormatGbiMappings','ge_bg_texture_gbi_formats'),
                            ('g_TexFormatDepths','ge_bg_texture_gbi_depths'),
                            ('g_TexFormatLutModes','ge_bg_texture_lut_modes')):
        match=re.search(r's32 '+original+r'\[\]\s*=\s*\{[^}]+\};',source)
        assert match is not None
        out.append(match[0].replace('s32 '+original,'static const int32_t '+native))
    out += ['typedef struct GeBgTextureMetadata {',
            '    uint8_t format, maxlod, explicit_lods, palette;',
            '    uint8_t widths[8], heights[8];',
            '} GeBgTextureMetadata;',
            'static const GeBgTextureMetadata ge_bg_texture_metadata[] = {']
    for fmt,lod,explicit,palette,widths,heights in metadata:
        out.append('    {%d,%d,%d,%d,{%s},{%s}},' % (fmt,lod,explicit,palette,
            ','.join(str(x) for x in widths),','.join(str(x) for x in heights)))
    out += ['};','']
    return '\n'.join(out)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('repo',type=Path);p.add_argument('images',type=Path);p.add_argument('catalog',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.write_text(generate(a.repo,a.images,a.catalog))
