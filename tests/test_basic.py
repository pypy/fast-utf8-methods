# coding: utf-8
import struct
from tests.support import AbstractUnicodeTestCase
from hypothesis import given, example
from hypothesis import settings
from hypothesis import strategies as st

def _utf8_check(bytestring):
    try:
        decoded = bytestring.decode('utf-8')
        result = len(decoded)
    except UnicodeDecodeError:
        result = -1
    return result, bytestring

class TestBasicFunctions(AbstractUnicodeTestCase):
    c_filename = 'utf8.c'
    cdef = """
    typedef struct decoding_error {
        size_t start;
        size_t end;
        int reason_index;
    } decoding_error_t;

    ssize_t count_utf8_codepoints(const uint8_t * encoded, size_t len, decoding_error_t * error);
    ssize_t count_utf8_codepoints_slow(const uint8_t * encoded, size_t len, decoding_error_t * error);
    int _check_continuation(const uint8_t ** encoded, const uint8_t * endptr, int count);
    """

    @settings(timeout=5, max_examples=2**5)
    @given(st.binary())
    def test_check_utf8_slow(self, bytestring):
        error = self.ffi.new("decoding_error_t[1]")
        try:
            decoded = bytestring.decode('utf-8')
            result = len(decoded)
        except UnicodeDecodeError:
            result = -1
        assert self.lib.count_utf8_codepoints_slow(bytestring, len(bytestring), error) == result

    #def test_check_utf8_codepoint_range(self):
    #    error = self.ffi.new("decoding_error_t[1]")
    #    for i in range(2**32):
    #        bytestring = struct.pack('I', i)
    #        try:
    #            decoded = bytestring.decode('utf-8')
    #            result = len(decoded)
    #        except UnicodeDecodeError:
    #            result = -1
    #        assert self.lib.count_utf8_codepoints_slow(bytestring, len(bytestring), error) == result

    def test_check_example(self):
        error = self.ffi.new("decoding_error_t[1]")
        assert self.lib.count_utf8_codepoints_slow(b"\xe1\x80\x80", 3, error) == 1

    @given(bytestring=st.binary(min_size=16, max_size=16))
    def test_check_correct_utf8_fast(self, bytestring):
        error = self.ffi.new("decoding_error_t[1]")
        check = lambda b: self.lib.count_utf8_codepoints(b, len(b), error)
        result, bytestring = _utf8_check(bytestring)
        assert check(bytestring) == result

    def test_test(self):
        error = self.ffi.new("decoding_error_t[1]")

        check = lambda b: self.lib.count_utf8_codepoints(b, len(b), error)

        #ss = b'\xe0\xa0\x80'+b'\xe0\xa0\x80'+b'\xf8\x8f\x80\x80'+b'\xed\x9f\x80\x00\xc2\x80'
        ss = b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xc2\x80'
        result, bytestring = _utf8_check(ss)
        assert check(bytestring) == result

    def test_boundary_cases(self):
        error = self.ffi.new("decoding_error_t[1]")

        check = lambda b: self.lib.count_utf8_codepoints(b, len(b), error)

        ss = b'\x00'*15 + b'\xe2\x80\x80'
        result, bytestring = _utf8_check(ss)
        assert check(bytestring) == result

        ss = b'\x00'*13 + b'\xf2\x80\x80\x80'
        result, bytestring = _utf8_check(ss)
        assert check(bytestring) == result

        ss = b'\x00'*14 + b'\xe2\x80\x80'
        result, bytestring = _utf8_check(ss)
        assert check(bytestring) == result
