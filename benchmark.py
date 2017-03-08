import perf
import sys
from os.path import dirname, join
from cffi import FFI
import importlib

def compile_ffi(cdef, files, modulename, defines=[], includes=[],
                verbose=False, link_flags=[]):
    ffi = FFI()
    ffi.cdef(cdef)
    directory = dirname(dirname(__file__)) # two levels below!
    if isinstance(files, str):
        filename = files
        with open(join(directory, 'src', filename), 'rb') as fd:
            source = fd.read().decode()
    elif isinstance(files, list):
        assert len(files) > 1
        filename = files[0]
        source = ""
        for f in files:
            with open(join(directory, 'src', f), 'rb') as fd:
                source += fd.read().decode()
    else:
        raise NotImplementedError
    if sys.version_info[0] == 2:
        define_allow_surrogates = "-DALLOW_SURROGATES=1"
    else:
        define_allow_surrogates = "-DALLOW_SURROGATES=0"
    ffi.set_source(modulename, source,
            include_dirs=['src'],
            extra_compile_args=['-O3', define_allow_surrogates],
            extra_link_args=[],
            extra_objects=['src/utf8-sse4.o', 'src/utf8-avx.o'])
    ffi.compile(verbose=verbose)
    _test = importlib.import_module(modulename)
    return _test.ffi, _test.lib

ffi, lib = compile_ffi("""
double _bench_seq(const uint8_t * bytes, int len);
double _bench_vec_sse4(const uint8_t * bytes, int len);
double _bench_vec_avx2(const uint8_t * bytes, int len);
double _bench_libunistring(const uint8_t * bytes, int len);
double _bench_mystringlenutf8(const uint8_t * bytes, int len);
""", 'bench.c', 'bench', verbose=False)


def run(loops, data, func):
    cycles = 0
    for i in range(loops):
        cycs = func(data, len(data))
        cycles += cycs
    return cycles

def run_file(runner, name, filename):
    with open(filename, 'rb') as fd:
        data = fd.read()
        il = 10
        runner.bench_sample_func('pypy-seq-'+name, run, data, lib._bench_seq, inner_loops=il)
        runner.bench_sample_func('pypy-sse4-vec-'+name, run, data, lib._bench_vec_sse4, inner_loops=il)
        runner.bench_sample_func('pypy-avx2-vec-'+name, run, data, lib._bench_vec_avx2, inner_loops=il)
        runner.bench_sample_func('libunistring-'+name, run, data, lib._bench_libunistring, inner_loops=il)
        # Just length checking, no validation http://www.daemonology.net/blog/2008-06-05-faster-utf8-strlen.html
        runner.bench_sample_func('mystrlenutf8-'+name, run, data, lib._bench_mystringlenutf8, inner_loops=il)

runner = perf.Runner()
# a news website containing german umlauts and some other unicode chars, but expected mostly ascii
#run_file(runner, 'news-de', 'tests/html/derstandard.at.html')
#run_file(runner, 'news-cn', 'tests/html/worldjournal.cn.html')
run_file(runner, 'tipitaka-thai', 'tests/html/tipitaka-thai.html')
