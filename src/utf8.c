#include "utf8.h"

#include <stdio.h>
#include <xmmintrin.h>
#include <smmintrin.h>

void _print_mmx(const char * msg, __m128i chunk)
{
    printf("%s:", msg);
    // unpack the first 8 bytes, padding with zeros
    uint64_t a = _mm_extract_epi64(chunk, 0);
    uint64_t b = _mm_extract_epi64(chunk, 1);
    printf("%.2x%.2x%.2x%.2x %.2x%.2x%.2x%.2x  %.2x%.2x%.2x%.2x %.2x%.2x%.2x%.2x",
            (unsigned char)((a >> 0) & 0xff),
            (unsigned char)((a >> 8) & 0xff),
            (unsigned char)((a >> 16) & 0xff),
            (unsigned char)((a >> 24) & 0xff),

            (unsigned char)((a >> 32) & 0xff),
            (unsigned char)((a >> 40) & 0xff),
            (unsigned char)((a >> 48) & 0xff),
            (unsigned char)((a >> 56) & 0xff),

            (unsigned char)((b >> 0) & 0xff),
            (unsigned char)((b >> 8) & 0xff),
            (unsigned char)((b >> 16) & 0xff),
            (unsigned char)((b >> 24) & 0xff),

            (unsigned char)((b >> 32) & 0xff),
            (unsigned char)((b >> 40) & 0xff),
            (unsigned char)((b >> 48) & 0xff),
            (unsigned char)((b >> 56) & 0xff)
     );

    printf("\n");
}

ssize_t count_utf8_codepoints(const uint8_t * encoded, size_t len, decoding_error_t * error)
{
    size_t num_codepoints = 0;
    __m128i chunk;

    if (len == 0) {
        return 0;
    }
    __m128i zero = _mm_set1_epi8(0x00);
    __m128i one = _mm_set1_epi8(0x1);

    while (len >= 16) {
        chunk = _mm_loadu_si128((__m128i*)encoded);
        if (_mm_movemask_epi8(chunk) == 0) {
            // valid ascii chars!
            len -= 16;
            encoded += 16;
            num_codepoints += 16;
            continue;
        }

        //_print_mmx("chunk", chunk);
        // fight against the fact that there is no comparison on unsigned values
        __m128i chunk_signed = _mm_add_epi8(chunk, _mm_set1_epi8(0x80));
        //_print_mmx("shunk", chunk_signed);

        // ERROR checking
        // checking procedure works the following way:
        //
        // 1) mark all continuation bytes with either 0x1, 0x3, 0x7 (one, two or three bytes continuation)
        // 2) then check that there is no byte that has an invalid continuation
        __m128i twobytemarker = _mm_cmplt_epi8(_mm_set1_epi8(0xc0-1-0x80), chunk_signed);
        __m128i threebytemarker = _mm_cmplt_epi8(_mm_set1_epi8(0xe0-1-0x80), chunk_signed);
        __m128i fourbytemarker = _mm_cmplt_epi8(_mm_set1_epi8(0xf0-1-0x80), chunk_signed);

        // check that 0xc0 > 0xc2
        __m128i validtwobm = _mm_cmplt_epi8(_mm_set1_epi8(0xc2-1-0x80), chunk_signed);
        if (_mm_movemask_epi8(_mm_xor_si128(validtwobm, twobytemarker)) != 0) {
            // two byte marker should not be in range [0xc0-0xc2)
            return -1;
        }

        __m128i state2 = _mm_andnot_si128(threebytemarker, twobytemarker);
        __m128i contbytes = _mm_slli_si128(_mm_blendv_epi8(state2, one, twobytemarker), 1);

        if (_mm_movemask_epi8(threebytemarker) != 0) {
            // contain a 3 byte marker
            __m128i istate3 = _mm_andnot_si128(fourbytemarker, threebytemarker);
            __m128i state3 = _mm_slli_si128(_mm_blendv_epi8(zero, _mm_set1_epi8(0x3), istate3), 1);
            state3 = _mm_or_si128(state3, _mm_slli_si128(state3, 1));

            contbytes = _mm_or_si128(contbytes, state3);

            // range check
            __m128i equal_e0 = _mm_cmpeq_epi8(_mm_blendv_epi8(zero, chunk_signed, istate3),
                                              _mm_set1_epi8(0xe0-0x80));
            if (_mm_movemask_epi8(equal_e0) != 0) {
                __m128i mask = _mm_blendv_epi8(_mm_set1_epi8(0x7f), chunk_signed, _mm_slli_si128(equal_e0, 1));
                __m128i check_surrogate = _mm_cmplt_epi8(mask, _mm_set1_epi8(0xa0-0x80));
                if (_mm_movemask_epi8(check_surrogate) != 0) {
                    // invalid surrograte character!!!
                    return -1;
                }
            }

            // verify that there are now surrogates
            if (!ALLOW_SURROGATES) {
                __m128i equal_ed = _mm_cmpeq_epi8(_mm_blendv_epi8(zero, chunk_signed, istate3),
                                                  _mm_set1_epi8(0xed-0x80));
                if (_mm_movemask_epi8(equal_ed) != 0) {
                    __m128i mask = _mm_blendv_epi8(_mm_set1_epi8(0x80), chunk_signed, _mm_slli_si128(equal_ed, 1));
                    __m128i check_surrogate = _mm_cmpgt_epi8(mask, _mm_set1_epi8(0xa0-1-0x80));
                    if (_mm_movemask_epi8(check_surrogate) != 0) {
                        // invalid surrograte character!!!
                        return -1;
                    }
                }
            }
        }
        if (_mm_movemask_epi8(fourbytemarker) != 0) {
            // contain a 4 byte marker
            __m128i istate4 = _mm_slli_si128(_mm_blendv_epi8(zero, _mm_set1_epi8(0x7), fourbytemarker), 1);
            __m128i state4 =_mm_or_si128(istate4, _mm_slli_si128(istate4, 1));
            state4 =_mm_or_si128(state4, _mm_slli_si128(istate4, 2));

            contbytes = _mm_or_si128(contbytes, state4);

            // range check, filter out f0 and 
            __m128i equal_f0 = _mm_cmpeq_epi8(_mm_blendv_epi8(zero, chunk_signed, fourbytemarker),
                                              _mm_set1_epi8(0xf0-0x80));
            if (_mm_movemask_epi8(equal_f0) != 0) {
                __m128i mask = _mm_blendv_epi8(_mm_set1_epi8(0x7f), chunk_signed, _mm_slli_si128(equal_f0, 1));
                __m128i check_surrogate = _mm_cmplt_epi8(mask, _mm_set1_epi8(0x90-0x80));
                if (_mm_movemask_epi8(check_surrogate) != 0) {
                    return -1;
                }
            }

            __m128i equal_f4 = _mm_cmpeq_epi8(_mm_blendv_epi8(zero, chunk_signed, fourbytemarker),
                                              _mm_set1_epi8(0xf4-0x80));
            if (_mm_movemask_epi8(equal_f4) != 0) {
                __m128i mask = _mm_blendv_epi8(_mm_set1_epi8(0x80), chunk_signed, _mm_slli_si128(equal_f4, 1));
                __m128i check_surrogate = _mm_cmpgt_epi8(mask, _mm_set1_epi8(0x90-1-0x80));
                if (_mm_movemask_epi8(check_surrogate) != 0) {
                    return -1;
                }
            }

            __m128i equal_f5_gt = _mm_cmpgt_epi8(_mm_blendv_epi8(zero, chunk_signed, fourbytemarker),
                                              _mm_set1_epi8(0xf4-0x80));
            if (_mm_movemask_epi8(equal_f5_gt) != 0) {
                return -1;
            }
        }

        __m128i check_cont = _mm_cmpgt_epi8(contbytes, zero);
        __m128i contpos = _mm_and_si128(_mm_set1_epi8(0xc0), chunk);
        contpos = _mm_cmpeq_epi8(_mm_set1_epi8(0x80), contpos);
        //_print_mmx("contp", contpos);
        //_print_mmx("ccont", check_cont);
        __m128i validcont = _mm_xor_si128(check_cont, contpos);
        //_print_mmx("valid", validcont);
        if (_mm_movemask_epi8(validcont) != 0) {
            // uff, nope, that is really not utf8
            return -1;
        }

        // CORRECT, calculate the length
        __m128i count = _mm_set1_epi8(0xc1);
        __m128i is_continuation = _mm_and_si128(_mm_set1_epi8(0xc0), chunk_signed);
        __m128i cont_spots = _mm_cmpeq_epi8(zero, is_continuation);
        count =  _mm_and_si128(count, _mm_set1_epi8(0x7));

        //__m128i cond2 = _mm_cmplt_epi8(_mm_set1_epi8(0xc2-1-0x80), chunk_signed);
        // copy 0x00 over to each place which is a continuation byte
        count = _mm_blendv_epi8(count, zero, cont_spots);
        //count = _mm_blendv_epi8(count, zero,  cond2);
        //#count = _mm_subs_epu8(count, _mm_set1_epi8(0x1));

        // count the code points using 2x 32 bit hadd and one last 16 hadd
        // the result will end up at the lowest position
        count = _mm_hadd_epi32(count, count);
        count = _mm_hadd_epi32(count, count);
        count = _mm_hadd_epi16(count, count);
        uint16_t c = _mm_extract_epi16(count, 0);

        num_codepoints += (c & 0xff) + ((c >> 8) & 0xff);
        len -= 16;
        encoded += 16;
    }

    if (len == 0) {
        return num_codepoints;
    }

    return num_codepoints + count_utf8_codepoints_slow(encoded, len, error);
}

int _check_continuation(const uint8_t ** encoded, const uint8_t * endptr, int count) {
    ssize_t size = endptr - *encoded;

    if (size < count) {
        // not enough bytes to be a valid 2 byte utf8 code point
        return -1;
    }
    for (int i = 0; i < count; i++) {
        uint8_t byte = *(*encoded)++;
        if ((byte & 0xc0) != 0x80) { 
            // continuation byte does NOT match 0x10xxxxxx
            return -1;
        }
    }
    return 0;
}

ssize_t count_utf8_codepoints_slow(const uint8_t * encoded, size_t len, decoding_error_t * error) {
    size_t num_codepoints = 0;
    uint8_t byte = 0;
    const uint8_t * endptr = encoded + len;

    while (encoded < endptr) {
        byte = *encoded++;
        if (byte < 0x80) {
            num_codepoints += 1;
            continue;
        } else {
                //asm("int $3");
            if ((byte & 0xe0) == 0xc0) {
                // one continuation byte
                if (byte < 0xc2) {
                    return -1;
                }
                if (_check_continuation(&encoded, endptr, 1) != 0) {
                    return -1;
                }
            } else if ((byte & 0xf0) == 0xe0) {
                // two continuation byte
                if (_check_continuation(&encoded, endptr, 2) != 0) {
                    return -1;
                }
                uint8_t byte1 = encoded[-2];
                //surrogates shouldn't be valid UTF-8!
                if ((byte == 0xe0 && byte1 < 0xa0) ||
                    (byte == 0xed && byte1 > 0x9f && !ALLOW_SURROGATES)) {
                    return -1;
                }
            } else if ((byte & 0xf8) == 0xf0) {
                // three continuation byte
                if (_check_continuation(&encoded, endptr, 3) != 0) {
                    return -1;
                }
                uint8_t byte1 = encoded[-3];
                if ((byte == 0xf0 && byte1 < 0x90) ||
                    (byte == 0xf4 && byte1 > 0x8f) ||
                    (byte >= 0xf5)) {
                    return -1;
                }
            } else {
                // TODO
                return -1;
            }
            num_codepoints += 1;
        }
    }
    return num_codepoints;
}
