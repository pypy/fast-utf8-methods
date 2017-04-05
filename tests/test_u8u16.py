# coding: utf-8
from tests.support import AbstractUnicodeTestCase, _utf8_check
from hypothesis import given, example
from hypothesis import settings
from hypothesis import strategies as st
from os import walk, getcwd
from os.path import join, exists

class TestBasicFunctions(AbstractUnicodeTestCase):
    c_filename = 'utf8.c'
    cdef = """
    ssize_t fu8_count_utf8_codepoints(const uint8_t * encoded, size_t len);
    ssize_t fu8_count_utf8_codepoints_seq(const uint8_t * encoded, size_t len);
    extern ssize_t fu8_count_utf8_codepoints_sse4(const uint8_t * encoded, size_t len);
    extern ssize_t fu8_count_utf8_codepoints_avx(const uint8_t * encoded, size_t len);
    """

    def setup_class(clz):
        AbstractUnicodeTestCase.setup_class(clz)
        path = join(getcwd(), 'thirdparty','u8u16','QA','TestFiles')
        assert exists(path), "please see section in readme on how to download u8u16"
        clz.test_files = []
        for root, dirs, files in walk(path):
            del dirs[:]
            for file in files:
                clz.test_files.append(join(root, file))

    @given(st.data())
    def test_u8u16(self, data):
        for path in self.test_files:
                with open(path, 'rb') as fd:
                    bytestring = fd.read()
                    a = data.draw(st.integers(min_value=0, max_value=len(bytestring)))
                    b = data.draw(st.integers(min_value=a, max_value=len(bytestring)))
                    result, bytestring = _utf8_check(bytestring[a:b])
                    assert self.check(bytestring) == result

