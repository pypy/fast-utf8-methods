import perf
import sys
from os.path import dirname, join
from cffi import FFI
import importlib
import numpy
from tests.test_index import itab

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
double _bench_seq(const char * bytes, int len);
double _bench_vec_sse4(const char * bytes, int len);
double _bench_vec_avx2(const char * bytes, int len);
double _bench_libunistring(const char * bytes, int len);
double _bench_mystringlenutf8(const char * bytes, int len);

struct fu8_idxtab;
typedef struct fu8_idxtab fu8_idxtab_t;
double _bench_index_seq(ssize_t, const char *, int, ssize_t, fu8_idxtab_t**);
double _bench_index_sse4(ssize_t, const char *, int, ssize_t, fu8_idxtab_t**);
double _bench_index_avx2(ssize_t, const char *, int, ssize_t, fu8_idxtab_t**);

ssize_t fu8_count_utf8_codepoints(const char * bytes, ssize_t len);
void fu8_free_idxtab(struct fu8_idxtab *);
""", 'bench.c', 'bench', verbose=False)

def run(loops, data, func):
    clock = 0
    for i in range(loops):
        c = func(data, len(data))
        clock += c
    return clock

def inner_loop_index(loops, data, func):
    clock = 0
    cplen = lib.fu8_count_utf8_codepoints(data, len(data))
    assert cplen >= 0
    sigma = cplen/4
    mu = cplen/2
    with itab(ffi, lib) as t:
        for i in range(loops):
            i = numpy.random.normal(mu, sigma)
            if not 0 <= i < cplen:
                continue
            c = func(int(i), data, len(data), int(cplen), t)
            clock += c
    return clock

def run_check(runner, name, filename):
    with open(filename, 'rb') as fd:
        data = fd.read()
        il = 10
        runner.bench_sample_func('pypy-seq-'+name, run, data, lib._bench_seq, inner_loops=il)
        runner.bench_sample_func('pypy-sse4-vec-'+name, run, data, lib._bench_vec_sse4, inner_loops=il)
        runner.bench_sample_func('pypy-avx2-vec-'+name, run, data, lib._bench_vec_avx2, inner_loops=il)
        runner.bench_sample_func('libunistring-'+name, run, data, lib._bench_libunistring, inner_loops=il)
        # Just length checking, no validation http://www.daemonology.net/blog/2008-06-05-faster-utf8-strlen.html
        runner.bench_sample_func('mystrlenutf8-'+name, run, data, lib._bench_mystringlenutf8, inner_loops=il)

def run_index(runner, name, filename):
    with open(filename, 'rb') as fd:
        data = fd.read()
        il = 10
        runner.bench_sample_func('pypy-seq-'+name, inner_loop_index, data, lib._bench_index_seq, inner_loops=il)
        runner.bench_sample_func('pypy-sse4-'+name, inner_loop_index, data, lib._bench_index_sse4, inner_loops=il)
        runner.bench_sample_func('pypy-avx2-'+name, inner_loop_index, data, lib._bench_index_avx2, inner_loops=il)

runner = perf.Runner()
# a news website containing german umlauts and some other unicode chars, but expected mostly ascii
#run_check(runner, 'news-de', 'tests/html/derstandard.at.html')
#run_check(runner, 'news-cn', 'tests/html/worldjournal.cn.html')
#run_check(runner, 'tipitaka-thai', 'tests/html/tipitaka-thai.html')

run_index(runner, 'news-de', 'tests/html/derstandard.at.html')

