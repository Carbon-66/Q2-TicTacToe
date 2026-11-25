#!/usr/bin/env python3
"""Stub shared library to link q2-tictactoe against: it has glibc's soname and the symbols the game
imports, so lld writes the right NEEDED entry and dynamic relocations. On the Q2, the rootfs's own
glibc 2.28 resolves them. Usage: gen.py OUTDIR

Versions matter (as in Challenger Deep and q2-doom). An unversioned reference binds to a symbol's
oldest version, and on the Q2's MIPS glibc fopen and fclose have a GLIBC_2.0 compatibility version
(libio's old FILE layout) beside the current GLIBC_2.2 one. Bound to the old ones, stdio quietly
breaks. So those carry GLIBC_2.2, as stock demo imports them. If you add a libc call, add it here."""
import pathlib, subprocess, sys

LIBS = {
    'libc.so.6': {
        'data': ['stdout', 'stderr', 'stdin'],
        'versioned': {'GLIBC_2.2': ['fopen', 'fclose']},
        'text': '''__libc_start_main __errno_location bind calloc chdir clock_gettime close exit fflush
            fgets fprintf fputc fputs free freopen getenv ioctl malloc memcpy memmove memset mmap open printf
            recv rename setvbuf signal snprintf socket sscanf strchr strcmp strcpy strlen strrchr strtol time
            unlink usleep vfprintf'''.split(),
    },
}

out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for soname, syms in LIBS.items():
    c = out / (soname + '.c')
    script = out / (soname + '.map')
    versioned = syms.get('versioned', {})
    script.write_text(''.join(f'{v} {{ global: {"; ".join(names)}; }};\n' for v, names in versioned.items()))
    c.write_text(''.join(f'void *{d} = 0;\n' for d in syms['data']) +
                 ''.join(f'void {t}(void) {{}}\n' for t in syms['text'] + sum(versioned.values(), [])))
    subprocess.run(['clang', '--target=mipsel-linux-gnu', '-march=mips32r2', '-mabi=32', '-mfp64',
                    '-mnan=2008', '-fPIC', '-nostdlib', '-shared', '-fuse-ld=lld', '-fno-builtin', '-w',
                    f'-Wl,-soname,{soname}', *([f'-Wl,--version-script={script}'] if versioned else []), '-o', out / soname, c],
                   check=True)
