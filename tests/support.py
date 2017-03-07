import sys
from os.path import dirname, join
from cffi import FFI
import importlib

def _utf8_check(bytestring):
    try:
        decoded = bytestring.decode('utf-8')
        result = len(decoded)
    except UnicodeDecodeError:
        result = -1
    return result, bytestring

def compile_ffi(cdef, files, modulename, defines=[], includes=[], verbose=False):
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
            include_dirs=['src'], extra_compile_args=['-O3', define_allow_surrogates, '-msse4.1'])
    ffi.compile(verbose=verbose)
    _test = importlib.import_module(modulename)
    return _test.ffi, _test.lib

class AbstractUnicodeTestCase(object):
    c_filename = '.c'
    cdef = ""

    def setup_class(clz):
        modulename = "tests."+clz.__name__
        ffi, lib = compile_ffi(clz.cdef, clz.c_filename, modulename)
        clz.ffi = ffi
        clz.lib = lib
