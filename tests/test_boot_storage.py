"""Exercise actual frontend startup/card failures and overlay controls.

The SDK facade injects RPC/module failures; this is not PS2 emulation.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
cc = os.environ.get('CC', 'cc')
with tempfile.TemporaryDirectory() as directory:
    tmp = Path(directory)
    flags = ['-O1', '-g', '-std=gnu11', '-fsanitize=address,undefined',
             '-fno-omit-frame-pointer', '-fno-pie', '-no-pie', '-I.', '-Iinclude']
    env = os.environ.copy()
    # LeakSanitizer cannot inspect threads in some restricted runners; the
    # stack/address and undefined-behavior checks remain enabled.
    env['ASAN_OPTIONS'] = 'detect_leaks=0:detect_stack_use_after_return=1:halt_on_error=1'
    env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    for embedded, cases in [(True, [1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20]), (False, [9])]:
        obj = tmp / 'frontend.o'
        binary = tmp / 'boot'
        subprocess.run([cc, *flags, '-DSMW_LOOP_TEST', '-Dmain=port_main',
                        *(['-DSMW_EMBEDDED_ASSETS'] if embedded else []),
                        '-c', 'src/ps2_main.c', '-o', str(obj)], cwd=ROOT, check=True)
        subprocess.run([cc, *flags, '-Itests', '-DSMW_TEST_REAL_MENU', str(obj),
                        'tests/platform_mock.c', 'src/menu.c',
                        '-Wl,--wrap=menu_draw', '-Wl,--wrap=menu_text',
                        'src/frame_pacer.c', 'src/audio_queue.c', '-o', str(binary)],
                       cwd=ROOT, check=True)
        for case in cases:
            duration = '12' if case in (13, 14) else '6'
            run = subprocess.run([str(binary), duration, '0', '0', '0', '0', str(case)],
                                 cwd=tmp, capture_output=True, text=True, env=env, timeout=20)
            if run.returncode:
                print(run.stdout + run.stderr)
            assert run.returncode == 0, (case, run.returncode)
            for line in run.stdout.splitlines():
                if line.startswith(('BOOT_RESULT ', 'HANDOFF_RESULT ', 'IDLE_RESULT ', 'ISO_ERROR_RESULT ')):
                    print(line)
print('PASS: normal/unformatted/stuck/failed card RPC; delayed callback memory lifetime; '
      'reset deadlines; missing card module; OPL-like resident reuse; host: build; '
      'overlay initially hidden, L2+R2 toggle and launcher return, '
      'MX4SIO card save on either port, absent/timeout fallback, arcade without card I/O, repeated aspect switches and input handoff, '
      'clean ISO reset, retry and failure screens, MX4SIO input after 8.7 seconds idle')
