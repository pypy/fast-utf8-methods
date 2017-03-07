import perf
from tests.support import compile_ffi

ffi, lib = compile_ffi("""
unsigned long long _bench_seq(const uint8_t * bytes, int len);
""", 'src/utf8.c', 'pypyutf8',
['-DBENCHMARKS=1', '-Isrc'])

def run_seq(loops, data):
    cycles = 0
    for i in range(loops):
        cycs = lib._bench_seq(data, len(data), loops)
        cycles += cycs
    return cycles

runner = perf.Runner()
runner.bench_sample_func('pypy-utf8', run_seq, inner_loops=10)
