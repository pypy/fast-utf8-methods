import sys
from os.path import dirname, join
from cffi import FFI
import importlib

class AbstractUnicodeTestCase(object):
    c_filename = '.c'
    cdef = ""

    def setup_class(clz):
        ffi = FFI()
        ffi.cdef(clz.cdef)
        filename = dirname(dirname(__file__)) # two levels below!
        with open(join(filename, 'src', clz.c_filename), 'rb') as fd:
            source = fd.read().decode()
        modulename = "tests."+clz.__name__
        if sys.version_info[0] == 2:
            define_allow_surrogates = "-DALLOW_SURROGATES=1"
        else:
            define_allow_surrogates = "-DALLOW_SURROGATES=0"
        ffi.set_source(modulename, source,
                include_dirs=['src'], extra_compile_args=['-g', define_allow_surrogates, '-msse4.1'])
        ffi.compile(verbose=True)
        _test = importlib.import_module(modulename)
        clz.lib = _test.lib
        clz.ffi = _test.ffi
