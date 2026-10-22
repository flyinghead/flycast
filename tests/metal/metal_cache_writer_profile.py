#!/usr/bin/env python3
"""Compare cold application archives with/without their real background writer.

Uses the existing process lock to inhibit writing; no production behavior switch.
Apple's driver cache is neither deleted nor claimed cold. No app/window/ROM.
"""
from pathlib import Path
import argparse
import fcntl
import json
import os
import re
import statistics
import subprocess
import tempfile


def fields(line):
    return dict(re.findall(r'(\w+)=([^\s]+)', line))


def distribution(values):
    values = sorted(values)
    return dict(count=len(values), median=statistics.median(values),
                p95=values[int((len(values)-1)*.95)], maximum=max(values))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--trials', type=int, default=4)
    parser.add_argument('--api-validation', action='store_true')
    args = parser.parse_args()
    if not 1 <= args.trials <= 4:
        parser.error('trials must be 1..4')
    env = {k: v for k, v in os.environ.items() if not k.startswith('MTL_SHADER_VALIDATION')}
    env.update(MTL_DEBUG_LAYER=str(int(args.api_validation)), MTL_SHADER_VALIDATION='0')
    work = Path('/private/tmp/flycast-metal4-todo')
    work.mkdir(exist_ok=True)
    runs = []
    with tempfile.TemporaryDirectory(prefix='cache-transitions-writer-', dir=work) as directory:
        root = Path(directory)

        def run(name, lock_name=None):
            cache = root/name
            cache.mkdir()
            lock = None
            if lock_name:
                lock = (cache/lock_name).open('w')
                fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            try:
                result = subprocess.run([str(args.binary.resolve()), '--cache-profile',
                                         str(cache), 'optimized', 'cold'], env=env,
                                        capture_output=True, text=True, timeout=120)
            finally:
                if lock:
                    lock.close()
            text = result.stdout + result.stderr
            if result.returncode or re.search(r'^FAIL|error:', text, re.M):
                raise RuntimeError(f'{name}: exit {result.returncode}\n{text[-6000:]}')
            frames = [fields(s) for s in text.splitlines() if s.startswith('Metal cache frame ')]
            sync = [fields(s) for s in text.splitlines() if s.startswith('Metal cache synchronous ')]
            summary = json.loads(next(s.removeprefix('Metal cache summary ') for s in text.splitlines()
                                      if s.startswith('Metal cache summary ')))
            assert len(frames) == len(sync) == 384
            assert text.count('192 exact full-frame CPU-oracle images') == 2
            assert summary['pending'] == 0 and not summary['accepting'] and summary['timing']
            assert summary['hits'] == summary['probes'] == 0 and summary['misses'] > 0
            assert summary['copies'] == summary['queued'] == summary['recordings']
            if lock_name:
                assert summary['recordings'] == summary['segments'] == 0
                assert not list(cache.glob('*.cache'))
                # INFO_LOG may be compiled out in the native fixture. Verify
                # actual writer activity instead of relying on that message.
                assert all(summary['times'][f'writer_{i}']['calls'] == 0 for i in range(2))
            else:
                assert summary['recordings'] > 0 and summary['segments'] > 0
            samples = {}
            synchronous = {(s['mode'], s['frame']): s for s in sync}
            for mode in ['standard', 'OIT']:
                group = [f for f in frames if f['mode'] == mode]
                for label, selected in [('all', group), ('ordinary', [f for f in group if f['restart']=='0']),
                                        ('restart', [f for f in group if f['restart']=='1'])]:
                    samples[mode+'_'+label] = {
                        metric: distribution([float(f[metric]) for f in selected])
                        for metric in ['cpu_us', 'render_us', 'restart_us', 'gpu_ms', 'interval_us']}
            slowest = []
            for frame in sorted(frames, key=lambda f: float(f['render_us']), reverse=True)[:12]:
                slowest.append(dict(frame, synchronous=synchronous[(frame['mode'], frame['frame'])]))
            return dict(name=name, writer=not bool(lock_name), summary=summary,
                        frame_distributions=samples, slowest_render_frames=slowest,
                        synchronous_totals={key: sum(float(s[key]) for s in sync)
                                            for key in sync[0] if key not in ('mode', 'frame')}), cache

        # Discover the exact source/device-specific lock and prime driver state
        # once before alternating measured processes. Keep every app archive fresh.
        _, seed = run('identity-seed')
        locks = list(seed.glob('metal4_pipeline_*.lock'))
        assert len(locks) == 1
        for trial in range(args.trials):
            for writer in ([True, False] if trial % 2 == 0 else [False, True]):
                result, _ = run(f'{trial}-writer-{int(writer)}', None if writer else locks[0].name)
                result['trial'] = trial
                runs.append(result)
                print(json.dumps(dict(trial=trial, writer=writer, recordings=result['summary']['recordings'],
                                      misses=result['summary']['misses'])), flush=True)
    args.output.write_text(json.dumps(dict(
        api_validation=args.api_validation, shader_validation=False,
        application_archives='fresh empty directory for every process', system_cache_reset=False,
        method='One unmeasured priming run then alternating writer-on/read-only-lock processes. '
               'Foreground render/compute times exclude asynchronous material/writer times. '
               'CLI pacing is synthetic; these are not display intervals or game FPS.',
        exact_full_image_checks=(len(runs)+1)*384, runs=runs), indent=2)+'\n')
    print('PASS writer comparison:', len(runs), 'measured processes; temporary caches removed', flush=True)


if __name__ == '__main__':
    main()
