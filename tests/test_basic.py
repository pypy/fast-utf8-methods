# coding: utf-8
from tests.support import AbstractUnicodeTestCase, _utf8_check
from hypothesis import given, example
from hypothesis import settings
from hypothesis import strategies as st

class TestBasicFunctions(AbstractUnicodeTestCase):
    c_filename = 'utf8.c'
    cdef = """
    ssize_t count_utf8_codepoints(const uint8_t * encoded, size_t len);
    ssize_t count_utf8_codepoints_seq(const uint8_t * encoded, size_t len);
    int _check_continuation(const uint8_t ** encoded, const uint8_t * endptr, int count);
    extern ssize_t count_utf8_codepoints_sse4(const uint8_t * encoded, size_t len);
    extern ssize_t count_utf8_codepoints_avx(const uint8_t * encoded, size_t len);
    """


    @settings(timeout=5, max_examples=2**10)
    @given(st.binary())
    def test_check_utf8_seq(self, bytestring):
        try:
            decoded = bytestring.decode('utf-8')
            result = len(decoded)
        except UnicodeDecodeError:
            result = -1
        assert self.lib.count_utf8_codepoints_seq(bytestring, len(bytestring)) == result

    def test_check_example(self):
        assert self.lib.count_utf8_codepoints_seq(b"\xe1\x80\x80", 3) == 1

    @settings(timeout=20, max_examples=2**32)
    @given(bytestring=st.binary(min_size=16, max_size=64))
    def test_check_correct_utf8_fast(self, bytestring):
        result, bytestring = _utf8_check(bytestring)
        assert self.check(bytestring) == result

    def test_test(self):
        ss = b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xc2\x80'
        result, bytestring = _utf8_check(ss)
        assert self.check(bytestring) == result

    def test_boundary_cases(self):
        ss = b'\x00'*15 + b'\xe2\x80\x80'
        result, bytestring = _utf8_check(ss)
        assert self.check(bytestring) == result

        ss = b'\x00'*13 + b'\xf2\x80\x80\x80'
        result, bytestring = _utf8_check(ss)
        assert self.check(bytestring) == result

        ss = b'\x00'*14 + b'\xe2\x80\x80'
        result, bytestring = _utf8_check(ss)
        assert self.check(bytestring) == result

    def test_boundary_cases2(self):

        ss = b'\x00'*15 + b'\xc2'
        result, bytestring = _utf8_check(ss)
        assert self.check(bytestring) == result

        ss = b'\x00'*14 + b'\xe2\x80\x80'
        result, bytestring = _utf8_check(ss)
        assert self.check(bytestring) == result

        ss = b'\x00'*14 + b'\xc0\x80'
        result, bytestring = _utf8_check(ss)
        assert self.check(bytestring) == result

        ss = b'\x00'*14 + b'\xe0\x80'
        result, bytestring = _utf8_check(ss)
        assert self.check(bytestring) == result

    def test_special_cases(self):
        ss = b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xc2\x81\x00\x00'
        result, bytestring = _utf8_check(ss)
        assert self.check(bytestring) == result

    # test takes very long, takes over 4 hours. assumption is that
    # hypothesis should find those cases!
    #def test_check_utf8_codepoint_range(self):
    #    for i in range(2**32):
    #        bytestring = struct.pack('I', i)
    #        try:
    #            decoded = bytestring.decode('utf-8')
    #            result = len(decoded)
    #        except UnicodeDecodeError:
    #            result = -1
    #        assert self.lib.count_utf8_codepoints_seq(bytestring, len(bytestring)) == result

