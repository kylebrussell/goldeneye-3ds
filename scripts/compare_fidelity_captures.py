#!/usr/bin/env python3
"""Seal existing captures and compare recorded N64/3DS state and pixels.

No emulator is launched and no reference execution is inferred from a filename.
See docs/MatchedReferenceCaptures.md for the manifest and state contracts.
"""
import argparse
import hashlib
import html
import json
import math
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ppm_to_png import write_png

KINDS = {'original-n64-emulator', 'original-n64-hardware',
         'port-3ds-emulator', 'port-3ds-hardware', 'synthetic', 'unknown'}
REQUIRED = ('source_rom', 'binary', 'input', 'retrace_trace', 'initial_state', 'state',
            'execution_log', 'settings', 'recipe')
CHANNELS = ('players', 'actors', 'doors', 'objectives', 'lighting', 'effects',
            'frontend', 'transitions')
LIMIT = ('Content hashes establish file identity, not authenticity of an execution. '
         'State and execution provenance are producer declarations. Pixel metrics '
         'do not establish complete fidelity, stereo comfort, or hardware performance.')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, f'duplicate JSON key: {key}')
            result[key] = value
        return result
    def invalid(value):
        raise ValueError(f'non-finite JSON number: {value}')
    def finite_float(value):
        result = float(value)
        require(math.isfinite(result), f'non-finite JSON number: {value}')
        return result
    return json.loads(path.read_text(), object_pairs_hook=unique,
                      parse_constant=invalid, parse_float=finite_float)


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def validate_manifest(manifest):
    require(isinstance(manifest, dict) and type(manifest.get('schema')) is int
            and manifest['schema'] == 1,
            'capture manifest schema must be 1')
    execution = manifest.get('execution', {})
    require(isinstance(execution, dict) and execution.get('kind') in KINDS,
            'execution.kind must explicitly identify original, port, synthetic, or unknown')
    for key in ('producer', 'version', 'video_backend', 'state_method'):
        require(isinstance(execution.get(key), str) and execution[key].strip(),
                f'execution.{key} is required (use unknown when unavailable)')
    require('stereo_slider' in manifest and (manifest['stereo_slider'] is None
            or (number(manifest['stereo_slider']) and 0 <= manifest['stereo_slider'] <= 1)),
            'stereo_slider must be a finite number in [0, 1], or null for unknown')
    require(isinstance(manifest.get('artifacts'), dict), 'artifacts must be an object')
    captures = manifest.get('captures')
    require(isinstance(captures, list) and captures, 'at least one capture is required')
    eyes = set()
    for capture in captures:
        require(isinstance(capture, dict), 'capture must be an object')
        eye = capture.get('eye')
        require(eye in ('mono', 'left', 'right') and eye not in eyes,
                'capture eyes must be unique mono/left/right labels')
        eyes.add(eye)
        crop = capture.get('crop')
        require(isinstance(crop, list) and len(crop) == 4
                and all(type(v) is int for v in crop)
                and min(crop[:2]) >= 0 and min(crop[2:]) > 0,
                'crop must be explicit [x, y, width, height] integers')
        aspect = capture.get('pixel_aspect')
        require(isinstance(aspect, list) and len(aspect) == 2
                and all(type(v) is int and v > 0 for v in aspect),
                'pixel_aspect must be a positive integer ratio')
        require('tick' in capture and (capture['tick'] is None
                or (type(capture['tick']) is int and capture['tick'] >= 0)),
                'capture.tick must identify the nonnegative simulation tick rendered, or be null')


def file_records(manifest):
    for key, item in manifest['artifacts'].items():
        yield f'artifacts.{key}', item
    for item in manifest['captures']:
        yield f'captures.{item["eye"]}', item


def seal(descriptor, destination):
    manifest = load_json(descriptor)
    validate_manifest(manifest)
    for label, item in file_records(manifest):
        require(isinstance(item, dict) and isinstance(item.get('path'), str),
                f'{label}.path is required')
        path = (descriptor.parent / item['path']).resolve()
        require(path.is_file(), f'{label}: not a regular file: {path}')
        item.update(path=str(path), sha256=digest(path), bytes=path.stat().st_size)
    # Parse every image before sealing. In particular a crop must be in bounds.
    for capture in manifest['captures']:
        read_crop(capture)
    require(not destination.exists(), f'refusing to replace existing manifest: {destination}')
    with destination.open('x') as output:
        json.dump(manifest, output, indent=2, sort_keys=True, allow_nan=False)
        output.write('\n')
    return manifest


def verify(path):
    manifest = load_json(path)
    validate_manifest(manifest)
    for label, item in file_records(manifest):
        require(isinstance(item, dict) and isinstance(item.get('path'), str),
                f'{label}.path is required')
        source = (path.parent / item['path']).resolve()
        require(source.is_file(), f'{label}: missing file: {source}')
        require(type(item.get('bytes')) is int and source.stat().st_size == item['bytes']
                and re.fullmatch('[0-9a-f]{64}', str(item.get('sha256', '')))
                and digest(source) == item['sha256'], f'{label}: content hash/size mismatch')
        item['path'] = str(source)
    return manifest


def read_image(path):
    """P6 needs no dependencies; PNG optionally uses Pillow without conversion."""
    with path.open('rb') as source:
        signature = source.read(8)
    if signature.startswith(b'\x89PNG\r\n\x1a\n'):
        try:
            from PIL import Image
        except ImportError as error:
            raise ValueError('PNG input requires Pillow; P6 PPM needs no dependencies') from error
        with Image.open(path) as image:
            require(image.mode == 'RGB', 'PNG input must be opaque 8-bit RGB; export explicitly')
            return image.width, image.height, image.tobytes()
    data = path.read_bytes()
    # Bounded header tokenization: malformed/truncated PPM must not loop at EOF.
    position, tokens = 0, []
    while len(tokens) < 4:
        while position < len(data) and data[position:position + 1].isspace():
            position += 1
        if data[position:position + 1] == b'#':
            end = data.find(b'\n', position)
            require(end >= 0, 'unterminated PPM comment')
            position = end + 1
            continue
        start = position
        while position < len(data) and not data[position:position + 1].isspace():
            position += 1
        require(position > start and position < len(data), 'truncated PPM header')
        tokens.append(data[start:position])
    require(tokens[0] == b'P6' and tokens[3] == b'255', 'only 8-bit P6 PPM is supported')
    width, height = int(tokens[1]), int(tokens[2])
    require(width > 0 and height > 0, 'image dimensions must be positive')
    # Consume exactly one whitespace byte (CRLF is one header line ending).
    position += 2 if data[position:position + 2] == b'\r\n' else 1
    pixels = data[position:]
    require(len(pixels) == width * height * 3, 'PPM pixel byte count does not match dimensions')
    return width, height, pixels


def read_crop(capture):
    width, height, pixels = read_image(Path(capture['path']))
    x, y, cw, ch = capture['crop']
    require(x + cw <= width and y + ch <= height, 'capture crop exceeds image bounds')
    return cw, ch, b''.join(pixels[((y + row) * width + x) * 3:
                                 ((y + row) * width + x + cw) * 3] for row in range(ch))


def state_errors(state):
    errors = []
    if not isinstance(state, dict) or type(state.get('schema')) is not int or state['schema'] != 1:
        return ['state schema must be 1']
    for key in ('scene', 'clock', 'camera', 'channels'):
        if not isinstance(state.get(key), dict):
            errors.append(f'state.{key} must be an object')
    if errors:
        return errors
    for key in ('stage_id', 'difficulty', 'screen'):
        if state['scene'].get(key) is None:
            errors.append(f'state.scene.{key} is missing')
    clock = state['clock']
    if type(clock.get('tick')) is not int or clock['tick'] < 0:
        errors.append('state.clock.tick is invalid')
    if type(clock.get('hz')) is not int or clock['hz'] <= 0:
        errors.append('state.clock.hz is invalid')
    if clock.get('phase') != 'render-input':
        errors.append('state.clock.phase must be render-input')
    if not re.fullmatch('[0-9a-f]{64}', str(clock.get('retrace_trace_sha256', ''))):
        errors.append('state.clock.retrace_trace_sha256 is missing')
    camera = state['camera']
    if camera.get('convention') != 'n64-authoring-row-major':
        errors.append('state.camera.convention must be n64-authoring-row-major')
    for key in ('view', 'projection'):
        values = camera.get(key)
        if not isinstance(values, list) or len(values) != 16 or not all(map(number, values)):
            errors.append(f'state.camera.{key} must contain 16 finite numbers')
    if not isinstance(state.get('rng'), list) or not state['rng'] or not all(
            type(v) is int and v >= 0 for v in state['rng']):
        errors.append('state.rng must record every PRNG stream')
    for key in CHANNELS:
        if not isinstance(state['channels'].get(key), (dict, list)):
            errors.append(f'state.channels.{key} must be an explicit object or array')
    return errors


def differences(left, right, prefix='state'):
    """Report exact differing paths; do not silently tolerate camera/state drift."""
    if isinstance(left, dict) and isinstance(right, dict):
        result = []
        for key in sorted(left.keys() | right.keys()):
            if key not in left or key not in right:
                result.append(prefix + '.' + key)
            else:
                result.extend(differences(left[key], right[key], prefix + '.' + key))
        return result
    if left != right or (isinstance(left, bool) != isinstance(right, bool)):
        return [prefix]
    return []


def matching_gate(reference, candidate):
    reasons = []
    if not reference['execution']['kind'].startswith('original-n64-'):
        reasons.append('reference is not declared original N64 execution')
    if not candidate['execution']['kind'].startswith('port-3ds-'):
        reasons.append('candidate is not declared 3DS port execution')
    for name, manifest in (('reference', reference), ('candidate', candidate)):
        if manifest['execution']['state_method'] != 'runtime-instrumented':
            reasons.append(f'{name}: state was not runtime-instrumented')
        for key in ('producer', 'version', 'video_backend'):
            if manifest['execution'][key].lower() == 'unknown':
                reasons.append(f'{name}: execution.{key} is unknown')
        for key in REQUIRED + (('assets',) if name == 'candidate' else ()):
            if key not in manifest['artifacts'] or manifest['artifacts'][key]['bytes'] == 0:
                reasons.append(f'{name}: missing/empty artifact {key}')
        if manifest['stereo_slider'] != 0:
            reasons.append(f'{name}: unknown/nonzero stereo cannot be an N64 mono pixel comparison')
    for key in ('source_rom', 'input', 'retrace_trace'):
        a, b = reference['artifacts'].get(key), candidate['artifacts'].get(key)
        if a and b and a['sha256'] != b['sha256']:
            reasons.append(f'{key}: reference and candidate hashes differ')
    rom, binary = (reference['artifacts'].get(key) for key in ('source_rom', 'binary'))
    if rom and binary and rom['sha256'] != binary['sha256']:
        reasons.append('reference binary differs from declared original source ROM')
    for key in ('initial_state', 'state'):
        states = []
        for name, manifest in (('reference', reference), ('candidate', candidate)):
            artifact = manifest['artifacts'].get(key)
            if not artifact:
                states.append(None)
                continue
            state = load_json(Path(artifact['path']))
            reasons.extend(f'{name}.{key}: {error}' for error in state_errors(state))
            states.append(state)
            if key == 'state' and isinstance(state, dict) and isinstance(state.get('clock'), dict):
                retraces = manifest['artifacts'].get('retrace_trace')
                if retraces and state['clock'].get('retrace_trace_sha256') != retraces['sha256']:
                    reasons.append(f'{name}: state retrace trace hash differs from artifact')
                for capture in manifest['captures']:
                    if capture['tick'] != state['clock'].get('tick'):
                        reasons.append(f'{name}.{capture["eye"]}: capture tick differs from state')
        if all(state is not None for state in states):
            reasons.extend(differences(*states, prefix=key))
    return reasons


def pixel_metrics(left, right):
    require(len(left) == len(right) and len(left) > 0, 'pixel arrays must have equal nonzero sizes')
    delta = bytes(abs(a - b) for a, b in zip(left, right))
    count = len(delta) // 3
    return {'mean_absolute_error_8bit': sum(delta) / len(delta),
            'root_mean_square_error_8bit': math.sqrt(sum(v * v for v in delta) / len(delta)),
            'max_channel_error_8bit': max(delta),
            'changed_pixels': sum(any(delta[i:i + 3]) for i in range(0, len(delta), 3)),
            'pixels': count}, delta


def compare(reference_path, candidate_path, output, require_matched=False, max_mae=None,
            reference_eye='mono'):
    reference, candidate = verify(reference_path), verify(candidate_path)
    reasons = matching_gate(reference, candidate)
    require(not output.exists(), f'refusing to replace existing report directory: {output}')
    mono = next((c for c in reference['captures'] if c['eye'] == reference_eye), None)
    require(mono is not None, f'reference has no {reference_eye} capture')
    if reference_eye != 'mono':
        reasons.append('reference eye is not original N64 mono')
    rw, rh, rp = read_crop(mono)
    pairs = []
    prepared = []
    for capture in candidate['captures']:
        cw, ch, cp = read_crop(capture)
        aspect_a, aspect_b = mono['pixel_aspect'], capture['pixel_aspect']
        require((rw, rh) == (cw, ch), 'crop dimensions differ; no implicit resize is allowed')
        require(aspect_a[0] * aspect_b[1] == aspect_b[0] * aspect_a[1],
                'pixel aspects differ; no implicit geometry correction is allowed')
        metrics, delta = pixel_metrics(rp, cp)
        pairs.append({'reference_eye': reference_eye, 'candidate_eye': capture['eye'],
                      'reference_crop': mono['crop'], 'candidate_crop': capture['crop'],
                      'metrics': metrics})
        prepared.append((capture['eye'], cp, delta))
    report = {'schema': 1, 'matched_recorded_state': not reasons,
              'classification': 'matched-recorded-state' if not reasons else 'unmatched-exploratory',
              'mismatch_reasons': reasons, 'pairs': pairs, 'limit': LIMIT,
              'reference_manifest_sha256': digest(reference_path),
              'candidate_manifest_sha256': digest(candidate_path),
              'reference_execution': reference['execution'],
              'candidate_execution': candidate['execution'],
              'image_transform': 'explicit crop only; raw encoded RGB; difference shown at 1x'}
    report['acceptance'] = {'require_matched': require_matched or max_mae is not None,
                            'max_mae_8bit': max_mae,
                            'passed': (not (require_matched or max_mae is not None) or not reasons)
                            and (max_mae is None or all(
                                p['metrics']['mean_absolute_error_8bit'] <= max_mae for p in pairs))}
    output.mkdir(parents=True)
    write_png(output / 'reference.png', rw, rh, rp)
    rows = []
    for eye, cp, delta in prepared:
        write_png(output / f'{eye}.png', rw, rh, cp)
        write_png(output / f'{eye}-difference.png', rw, rh, delta)
        rows.append(f'<h2>Reference {reference_eye} / candidate {eye} / absolute difference (1x)</h2>'
                    f'<div><img src="reference.png" alt="reference">'
                    f'<img src="{eye}.png" alt="candidate {eye}">'
                    f'<img src="{eye}-difference.png" alt="absolute RGB difference"></div>')
    (output / 'report.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    (output / 'index.html').write_text('<!doctype html><meta charset="utf-8">'
        '<title>Capture comparison</title><style>body{font:16px system-ui;margin:2em;'
        'background:#181818;color:#eee}img{image-rendering:pixelated;margin:4px;max-width:32%}'
        'pre{white-space:pre-wrap}div{display:flex}</style><h1>'
        + html.escape(report['classification']) + '</h1><p>' + html.escape(LIMIT) + '</p>'
        + ''.join(rows) + '<pre>' + html.escape(json.dumps(report, indent=2)) + '</pre>')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    record = commands.add_parser('record', help='hash existing local capture evidence')
    record.add_argument('descriptor', type=Path)
    record.add_argument('manifest', type=Path)
    comparison = commands.add_parser('compare', help='validate evidence and render a static report')
    comparison.add_argument('reference', type=Path)
    comparison.add_argument('candidate', type=Path)
    comparison.add_argument('--out-dir', type=Path, required=True)
    comparison.add_argument('--reference-eye', choices=('mono', 'left', 'right'), default='mono',
                            help='left/right allow exploratory port-only comparisons')
    comparison.add_argument('--require-matched', action='store_true',
                            help='fail for missing original execution or differing recorded state')
    comparison.add_argument('--max-mae', type=float,
                            help='optional mean absolute RGB error gate; also requires matched state')
    args = parser.parse_args()
    try:
        if args.command == 'record':
            seal(args.descriptor, args.manifest)
            print(args.manifest)
            return 0
        require(args.max_mae is None or (math.isfinite(args.max_mae) and 0 <= args.max_mae <= 255),
                '--max-mae must be finite and in [0, 255]')
        report = compare(args.reference, args.candidate, args.out_dir,
                         args.require_matched, args.max_mae, args.reference_eye)
        print(json.dumps(report, indent=2, sort_keys=True))
        return 0 if report['acceptance']['passed'] else 1
    except (OSError, ValueError, TypeError, KeyError) as error:
        print(f'capture comparison: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
