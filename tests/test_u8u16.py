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
    typedef struct decoding_error {
        size_t start;
        size_t end;
        int reason_index;
    } decoding_error_t;

    ssize_t count_utf8_codepoints(const uint8_t * encoded, size_t len, decoding_error_t * error);
    ssize_t count_utf8_codepoints_seq(const uint8_t * encoded, size_t len, decoding_error_t * error);
    int _check_continuation(const uint8_t ** encoded, const uint8_t * endptr, int count);
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
    def test_u6u18(self, data):
        error = self.ffi.new("decoding_error_t[1]")
        check = lambda b: self.lib.count_utf8_codepoints(b, len(b), error)
        for path in self.test_files:
                with open(path, 'rb') as fd:
                    bytestring = fd.read()
                    a = data.draw(st.integers(min_value=0, max_value=len(bytestring)))
                    b = data.draw(st.integers(min_value=a, max_value=len(bytestring)))
                    result, bytestring = _utf8_check(bytestring[a:b])
                    assert check(bytestring) == result

