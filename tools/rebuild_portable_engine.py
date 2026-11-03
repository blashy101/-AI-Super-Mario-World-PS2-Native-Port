#!/usr/bin/env python3
"""Rebuild the portable native engine from source using a local PS2DEV SDK."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import build_from_rom as builder
from portable_engine import write_manifest, native_fingerprint

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 16:parser.error('--jobs must be 1..16')
    env = builder.sdk_environment()
    fingerprint = native_fingerprint(builder.ROOT)
    with tempfile.TemporaryDirectory(prefix='smwps2-engine-') as temporary:
        stage = Path(temporary) / 'source'
        builder.copy_source(builder.ROOT, stage)
        if native_fingerprint(stage) != fingerprint:
            raise RuntimeError('Native source changed while staging the engine build; retry.')
        blanks = stage / 'build/template-assets'
        blanks.mkdir(parents=True)
        for name, size in [('usa', 355868), ('japanese', 126408), ('super_system', 12812)]:
            (blanks / (name + '.blank')).write_bytes(bytes(size))
        subprocess.run(['make', '-j' + str(args.jobs), 'ps2',
                        'ASSET_CFLAGS=-fno-zero-initialized-in-bss',
                        'ASSET_FILE=build/template-assets/usa.blank',
                        'JAPANESE_ASSET_FILE=build/template-assets/japanese.blank',
                        'SUPER_SYSTEM_ASSET_FILE=build/template-assets/super_system.blank'],
                       cwd=stage, env=env, check=True)
        if native_fingerprint(builder.ROOT) != fingerprint:
            raise RuntimeError('Native source changed during compilation; retry before installing the engine.')
        folder = builder.ROOT / 'release/engine'
        folder.mkdir(parents=True, exist_ok=True)
        engine = folder / 'smw_engine.template'
        shutil.copyfile(stage / 'dist/SMWPS2.elf', engine)
        compiler = subprocess.check_output(['mips64r5900el-ps2-elf-gcc', '--version'], env=env, text=True).splitlines()[0]
        write_manifest(engine, folder / 'manifest.json', compiler)
    print('Rebuilt and verified the zero-asset engine from native source.')

if __name__ == '__main__':main()
