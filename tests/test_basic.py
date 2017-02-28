import struct
from tests.support import AbstractUnicodeTestCase
from hypothesis import given, example
from hypothesis import settings
from hypothesis import strategies as st

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

    @given(bytestring=st.binary())
    @example(bytestring="aaaabbbbccccdddd")
    def test_check_utf8_fast_single(self, bytestring):
        error = self.ffi.new("decoding_error_t[1]")
        try:
            decoded = bytestring.decode('utf-8')
            result = len(decoded)
        except UnicodeDecodeError:
            result = -1
        assert self.lib.count_utf8_codepoints(bytestring, len(bytestring), error) == result
