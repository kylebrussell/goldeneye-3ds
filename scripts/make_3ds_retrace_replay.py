#!/usr/bin/env python3
"""Replay a completed probe's canonical retrace deltas without changing its inputs."""
import argparse
from pathlib import Path


def replay_text(result: Path, capacity: int | None = None) -> str:
    fields = {}
    rows = []
    for line in result.read_text().splitlines():
        key, sep, value = line.partition('=')
        if key == 'frame_timing':
            rows.append([int(x) for x in value.split(',')])
        elif sep:
            fields[key] = value
    if fields.get('status') != 'complete' or int(fields.get('frame_timing_version', 0)) < 2:
        raise ValueError('a completed probe with measured retrace deltas is required')
    if not rows or len(rows) != int(fields.get('frame_timing_count', 0)):
        raise ValueError('missing frame records')
    if any(len(row) not in (13, 20) or row[0] != i+1 or not 1 <= row[-2] <= 255
           for i, row in enumerate(rows)):
        raise ValueError('invalid or nonsequential retrace records')
    capacity = len(rows) if capacity is None else capacity
    if not len(rows) <= capacity <= 60000:
        raise ValueError('capacity must cover the capture and fit the runtime probe limit')
    # An authored target route can finish before its configured frame limit.
    # Padding only supplies the unused tail; matched replays must still stop
    # at the same frame/checkpoints or the comparison gate rejects them.
    cadence = [row[-2] for row in rows] + [1]*(capacity-len(rows))
    return f'GE_RETRACE_REPLAY 1\nframes {capacity}\n' + ''.join(f'{x}\n' for x in cadence)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('result', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--frames', type=int, help='original configured limit for a route that finished early')
    args = parser.parse_args()
    try:
        text = replay_text(args.result, args.frames)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text)
