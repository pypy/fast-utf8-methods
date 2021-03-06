# coding: utf-8
from tests.support import AbstractUnicodeTestCase, _utf8_check
from hypothesis import given, example
from hypothesis import settings
from hypothesis import strategies as st
import pytest

class itab(object):
    def __init__(self, ffi, lib):
        self.ffi = ffi
        self.lib = lib

    def __enter__(self):
        self.itab = self.ffi.new("fu8_idxtab_t**")
        self.itab[0] = self.ffi.NULL
        return self.itab

    def __exit__(self, a, b, c):
        if self.itab[0] != self.ffi.NULL:
            p = self.itab[0]
            self.lib.fu8_free_idxtab(p)
        del self.itab

class TestBasicFunctions(AbstractUnicodeTestCase):
    c_filename = 'utf8.c'
    cdef = """
    typedef struct fu8_idxtab {
        int character_step;
        size_t * byte_positions;
        size_t bytepos_table_length;
    } fu8_idxtab_t;
    void fu8_free_idxtab(fu8_idxtab_t * t);
    ssize_t fu8_idx2bytepos(size_t cpidx, size_t cplen,
                            const uint8_t * utf8, size_t utf8len,
                            fu8_idxtab_t ** t);
    """

    def _build_utf8_idx2bytepos(self, text):
        idx2bytepos = {}
        bytelist = []
        pos = 0
        for i,uc in enumerate(text):
            ed = uc.encode('utf-8')
            bytelist.append(ed)
            idx2bytepos[i] = pos
            pos += len(ed)
        return idx2bytepos, b''.join(bytelist)

    @pytest.mark.parametrize('text,i,result', [
        ("0" * 10, 1, 1),
    ])
    def test_basic(self, text, i, result):
        idx2bytepos, utf8 = self._build_utf8_idx2bytepos(text)
        with itab(self.ffi, self.lib) as t:
            args = [i, len(text), utf8, len(utf8), t]
            assert self.lib.fu8_idx2bytepos(*args) == result

    @settings(timeout=5, max_examples=2**32)
    @given(text=st.text(average_size=150), data=st.data())
    def test_get_byte_pos_nth_codepoint(self, text, data):
        # + 1% longer indexing, to force out of bounds
        text = text * 10 # make it 10 times bigger
        i = data.draw(st.integers(min_value=0, max_value=len(text)+len(text)*0.01))
        idx2bytepos, utf8 = self._build_utf8_idx2bytepos(text)
        with itab(self.ffi, self.lib) as t:
            args = [i, len(text), utf8, len(utf8), t]
            result = self.lib.fu8_idx2bytepos(*args)
        if len(text) == 0:
            assert result == 0
        elif i >= len(text):
            assert result == -1
        else:
            assert result == idx2bytepos[i]

    @settings(timeout=60*10, max_examples=2**32)
    @given(text=st.text(average_size=150), data=st.data())
    def test_get_byte_pos_nth_codepoint_long(self, text, data):
        # + 1% longer indexing, to force out of bounds
        text = text * 10 # make it 10 times bigger
        idx2bytepos, utf8 = self._build_utf8_idx2bytepos(text)
        for j in range(1000):
            i = data.draw(st.integers(min_value=0, max_value=len(text)+len(text)*0.01))
            with itab(self.ffi, self.lib) as t:
                args = [i, len(text), utf8, len(utf8), t]
                result = self.lib.fu8_idx2bytepos(*args)
            if len(text) == 0:
                assert result == 0
            elif i >= len(text):
                assert result == -1
            else:
                assert result == idx2bytepos[i]

    @settings(timeout=60*10, max_examples=2**32)
    @given(text=st.text(average_size=250), data=st.data())
    def test_get_byte_pos_nth_codepoint_long_prebuild_table(self, text, data):
        text = text * 10 # make it 10 times bigger
        idx2bytepos, utf8 = self._build_utf8_idx2bytepos(text)
        # prebuild the table
        with itab(self.ffi, self.lib) as t:
            args = [max(len(text)-1, 0), len(text), utf8, len(utf8), t]
            result = self.lib.fu8_idx2bytepos(*args)

        for j in range(1000):
            i = data.draw(st.integers(min_value=0, max_value=len(text)+len(text)*0.01))
            with itab(self.ffi, self.lib) as t:
                args = [i, len(text), utf8, len(utf8), t]
                result = self.lib.fu8_idx2bytepos(*args)
            if len(text) == 0:
                assert result == 0
            elif i >= len(text):
                assert result == -1
            else:
                assert result == idx2bytepos[i]

    def test_test(self):
        text = "\n\U00019d1f\U00059a31\U000e75e0\U00088181\n>ᣮ!\U0004d62c\U001045d3\U000d614a.\x90옙Ǧ䰧唅B\U0001978e\x13\U0004ad76熍\x17\r쉵U!\x1b𩾺(\U0003a3e5\U000e29d9\U0003665a\x8c\U000e3dc4\U00072212ĐK\U000d36ad炁\U00107ae7\nÄ\x03踰\x08씡敗\x8fdMÎ뵫캖\U000ecb76\n\x1f#\x80\x17&淇犧'\U000d10f8v@\U000ee3aeB:\x06\n訞}ЋÍ䩦gÊ¶Ĝ\U000bb8eba5\\š\x99\U000552b6\x0fR\n\n\n\U000dcaab\x91\x94\x08\x132B뚸\nS\x13hᏝ荬쀞ᬽ^갓\U00012594Þ𦝑\U000cecd5\U000e0e19임신\U00098536챰3潓\U00014836頜\U0004c1fb\n*\t\n\nįkąۈ𠷇Å\x86\U00103c04캤\n/7?\U000546cb{ƉXÌ\U0008b219$WěģgĹ#\x08\x8c\U000d7432¶E\U000a8ef8弓\x0bG鰘]ǐ\n\U000e54b0\n\U000ace4f䲯Aㆱ\nꙈɓ*\U00089671\x11\nⅷR毟\n쀰⽽鍕\n\n𠷒ķ\U00091ef3\U000629994槥\x16⡮\n\x87\x12\U000bdee3" * 10
        i = 2037
        idx2bytepos, utf8 = self._build_utf8_idx2bytepos(text)
        with itab(self.ffi, self.lib) as t:
            args = [i, len(text), utf8, len(utf8), t]
            result2 = self.lib.fu8_idx2bytepos(*args)
            result = self.lib.fu8_idx2bytepos(*args)
            assert result2 == result
        if len(text) == 0:
            assert result == 0
        elif i >= len(text):
            assert result == -1
        else:
            assert result == idx2bytepos[i]

    def test_linear_index_table(self):
        text = "0" * 10000
        step = 1000
        with itab(self.ffi, self.lib) as t:
            assert self._check_index(text, 0, t) == 0
            assert t[0] == self.ffi.NULL
            assert self._check_index(text, step-40, t) == step-40
            assert t[0].byte_positions[0] == 0
            assert self._check_index(text, step+1, t) == step+1
            assert t[0].byte_positions[0] == step
            assert t[0].byte_positions[1] == 0
            assert self._check_index(text, 9999, t) == 9999
            assert self._check_index(text, 9999, t) == 9999
            for i in range(9):
                assert t[0].byte_positions[i] == (i+1)*step

        with itab(self.ffi, self.lib) as t:
            assert self._check_index(text, 0, t) == 0

    def _check_index(self, text, i, table):
        idx2bytepos, utf8 = self._build_utf8_idx2bytepos(text)
        args = [i, len(text), utf8, len(utf8), table]
        return self.lib.fu8_idx2bytepos(*args)

