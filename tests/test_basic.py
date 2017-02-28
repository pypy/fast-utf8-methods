from os.path import dirname, join
from cffi import FFI


class TestBasicFunctions(object):
    def setup_class(clz):
        ffi = FFI()
        ffi.cdef("""
        int check_length();
        """)
        filename = dirname(dirname(__file__))
        with open(join(filename, 'src', 'length.c'), 'rb') as fd:
            source = fd.read()
        ffi.set_source("tests._test_length", source,
                include_dirs=['src'])
        ffi.compile(verbose=True)
        from tests import _test_length
        clz.lib = _test_length.lib
        clz.ffi = _test_length.ffi



    def test_check_utf8(self):
        assert self.lib.check_length() == 0
