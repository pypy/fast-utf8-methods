import perf
from tests.support import compile_ffi

ffi, lib = compile_ffi("""
unsigned long long _bench_seq(const uint8_t * bytes, int len);
unsigned long long _bench_vec(const uint8_t * bytes, int len);
""", ['utf8.c','bench.c'], 'bench')

def run(loops, data, func):
    cycles = 0
    for i in range(loops):
        cycs = func(data, len(data))
        cycles += cycs
    return cycles

def run_file(runner, name, filename):
    with open(filename, 'rb') as fd:
        data = fd.read()
        runner.bench_sample_func('pypy-seq-'+name, run, data, lib._bench_seq, inner_loops=10)
        runner.bench_sample_func('pypy-vec-'+name, run, data, lib._bench_vec, inner_loops=10)

runner = perf.Runner()
# a news website containing german umlauts and some other unicode chars, but expected mostly ascii
run_file(runner, 'news-de', 'tests/html/derstandard.at.html')
