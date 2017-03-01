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

    def test_check_utf8_codepoint_range(self):
        error = self.ffi.new("decoding_error_t[1]")
        for i in xrange(2**32):
            bytestring = struct.pack('I', i)
            try:
                decoded = bytestring.decode('utf-8')
                result = len(decoded)
            except UnicodeDecodeError:
                result = -1
            assert self.lib.count_utf8_codepoints_slow(bytestring, len(bytestring), error) == result

    def test_check_example(self):
        error = self.ffi.new("decoding_error_t[1]")
        assert self.lib.count_utf8_codepoints_slow(b"\xe1\x80\x80", 3, error) == 1

    def test_check_utf8_fast_single(self):
        #@given(bytestring=st.binary())
        #@example(bytestring="aaaabbbbccccdddd")
        #@example(bytestring="\xc2\x80"*8)
        error = self.ffi.new("decoding_error_t[1]")

        check = lambda b: self.lib.count_utf8_codepoints(b, len(b), error)

        ss = ('\x00' * 14)+'\xc2\x80'
        result, bytestring = _utf8_check(ss)
        assert check(bytestring) == result

        #ss = u'ß'.encode('utf-8')
        #assert len(ss) == 2
        #result, bytestring = _utf8_check(ss * 8)
        #assert check(bytestring) == result

        #result, bytestring = _utf8_check("a"*16)
        #assert check(bytestring) == result


