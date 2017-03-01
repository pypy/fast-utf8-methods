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
    printf("%x%x%x%x %x%x%x%x  %x%x%x%x %x%x%x%x",
            (unsigned char)((a >> 56) & 0xff),
            (unsigned char)((a >> 48) & 0xff),
            (unsigned char)((a >> 40) & 0xff),
            (unsigned char)((a >> 32) & 0xff),

            (unsigned char)((a >> 24) & 0xff),
            (unsigned char)((a >> 16) & 0xff),
            (unsigned char)((a >> 8) & 0xff),
            (unsigned char)((a >> 0) & 0xff),


            (unsigned char)((b >> 56) & 0xff),
            (unsigned char)((b >> 48) & 0xff),
            (unsigned char)((b >> 40) & 0xff),
            (unsigned char)((b >> 32) & 0xff),

            (unsigned char)((b >> 24) & 0xff),
            (unsigned char)((b >> 16) & 0xff),
            (unsigned char)((b >> 8) & 0xff),
            (unsigned char)((b >> 0) & 0xff)
            );

    printf("\n");
}

ssize_t count_utf8_codepoints(const uint8_t * encoded, size_t len, decoding_error_t * error)
{
    size_t num_codepoints = 0;
    __m128i chunk_signed;

    if (len == 0) {
        return 0;
    }

    while (len >= 16) {
        chunk_signed = _mm_loadu_si128((__m128i*)encoded);
        if (_mm_movemask_epi8(chunk_signed) == 0) {
            len -= 16;
            encoded += 16;
            num_codepoints += 16;
            continue;
        }

        // state:          8080 8080  8080 8080
        __m128i state = _mm_set1_epi8(0x0 | 0x80);
        // mask:           4141 4141  4141 4141
        __m128i mask1 = _mm_set1_epi8(0xc2-1 -0x80);
        __m128i cond2 = _mm_cmplt_epi8(mask1, chunk_signed);
        _print_mmx("cond2", cond2);
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

ssize_t count_utf8_codepoints_slow(const uint8_t * encoded, size_t len, decoding_error_t * error)
{
    size_t num_codepoints = 0;
    uint8_t byte = 0, byte1, byte2, byte3;
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
