#include "utf8-private.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <immintrin.h>
#include <assert.h>

#define BIT(B,P) ((B >> (P-1)) & 0x1)

void _print_mmy(const char * msg, __m256i chunk)
{
    printf("%s:", msg);
    // unpack the first 8 bytes, padding with zeros
    uint64_t a = _mm256_extract_epi64(chunk, 0);
    uint64_t b = _mm256_extract_epi64(chunk, 1);
    uint64_t c = _mm256_extract_epi64(chunk, 2);
    uint64_t d = _mm256_extract_epi64(chunk, 3);
    printf("%.2x%.2x%.2x%.2x %.2x%.2x%.2x%.2x  %.2x%.2x%.2x%.2x %.2x%.2x%.2x%.2x    "
           "%.2x%.2x%.2x%.2x %.2x%.2x%.2x%.2x  %.2x%.2x%.2x%.2x %.2x%.2x%.2x%.2x",
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
            (unsigned char)((b >> 56) & 0xff),

            (unsigned char)((c >> 0) & 0xff),
            (unsigned char)((c >> 8) & 0xff),
            (unsigned char)((c >> 16) & 0xff),
            (unsigned char)((c >> 24) & 0xff),

            (unsigned char)((c >> 32) & 0xff),
            (unsigned char)((c >> 40) & 0xff),
            (unsigned char)((c >> 48) & 0xff),
            (unsigned char)((c >> 56) & 0xff),

            (unsigned char)((d >> 0) & 0xff),
            (unsigned char)((d >> 8) & 0xff),
            (unsigned char)((d >> 16) & 0xff),
            (unsigned char)((d >> 24) & 0xff),

            (unsigned char)((d >> 32) & 0xff),
            (unsigned char)((d >> 40) & 0xff),
            (unsigned char)((d >> 48) & 0xff),
            (unsigned char)((d >> 56) & 0xff)
     );

    printf("\n");
}

__m256i _shift_left1_mm256(__m256i r, __m256i zero) {
    // shift the individual 128bit parts, there might be several bytes missing
    __m128i low128 = _mm256_extracti128_si256(r, 0);
    __m256i v = _mm256_slli_si256(r, 1);
    __m256i w = _mm256_inserti128_si256(zero, low128, 1);
    // the low 128 shifted right!
    __m256i lors = _mm256_srli_si256(w, 15);
    return _mm256_or_si256(v, lors);
}

ssize_t fu8_count_utf8_codepoints_avx(const char * utf8, ssize_t len)
{
    const uint8_t * encoded = (const uint8_t*)utf8;
    __builtin_prefetch(encoded, 0, 0);
    size_t num_codepoints = 0;
    __m256i chunk;

    if (len <= 0) {
        return 0;
    }
    __m256i zero = _mm256_set1_epi8(0x00);
    while (len >= 32) {
        chunk = _mm256_loadu_si256((__m256i*)encoded);
        if (_mm256_movemask_epi8(chunk) == 0) {
            // valid ascii chars!
            len -= 32;
            encoded += 32;
            num_codepoints += 32;
            continue;
        }
        __builtin_prefetch(encoded+32, 0, 0);

        __m256i count = _mm256_set1_epi8(0x1);
        //_print_mmy("chunk", chunk);
        // fight against the fact that there is no comparison on unsigned values
        __m256i chunk_signed = _mm256_add_epi8(chunk, _mm256_set1_epi8(0x80));
        //_print_mmy("shunk", chunk_signed);

        // ERROR checking
        // checking procedure works the following way:
        //
        // 1) mark all continuation bytes with either 0x1, 0x3, 0x7 (one, two or three bytes continuation)
        // 2) then check that there is no byte that has an invalid continuation
        __m256i twobytemarker = _mm256_cmpgt_epi8(  chunk_signed, _mm256_set1_epi8(0xc0-1-0x80));
        __m256i threebytemarker = _mm256_cmpgt_epi8(chunk_signed, _mm256_set1_epi8(0xe0-1-0x80));
        __m256i fourbytemarker = _mm256_cmpgt_epi8( chunk_signed, _mm256_set1_epi8(0xf0-1-0x80));

        // the general idea of the following code collects 0xff for each byte position
        // in the variable contbytes.
        // at the end check if each position in contbytes set to 0xff is a valid continuation byte

        // check that 0xc0 > 0xc2
        __m256i validtwobm = _mm256_cmpgt_epi8(chunk_signed, _mm256_set1_epi8(0xc2-1-0x80));
        if (_mm256_movemask_epi8(_mm256_xor_si256(validtwobm, twobytemarker)) != 0) {
            // two byte marker should not be in range [0xc0-0xc2)
            return -1;
        }

        __m256i state2 = _mm256_andnot_si256(threebytemarker, twobytemarker);
        __m256i contbytes = _shift_left1_mm256(state2, zero);

        if (_mm256_movemask_epi8(threebytemarker) != 0) {
            // contains at least one 3 byte marker
            __m256i istate3 = _mm256_andnot_si256(fourbytemarker, threebytemarker);
            __m256i state3 = _shift_left1_mm256(istate3, zero);
            state3 = _mm256_or_si256(state3, _shift_left1_mm256(state3, zero));

            contbytes = _mm256_or_si256(contbytes, state3);

            // range check
            __m256i equal_e0 = _mm256_cmpeq_epi8(_mm256_blendv_epi8(zero, chunk_signed, istate3),
                                              _mm256_set1_epi8(0xe0-0x80));
            if (_mm256_movemask_epi8(equal_e0) != 0) {
                __m256i mask = _mm256_blendv_epi8(_mm256_set1_epi8(0x7f), chunk_signed, _shift_left1_mm256(equal_e0, zero));
                __m256i check_surrogate = _mm256_cmpgt_epi8(_mm256_set1_epi8(0xa0-0x80), mask); // lt
                if (_mm256_movemask_epi8(check_surrogate) != 0) {
                    // invalid surrograte character!!!
                    return -1;
                }
            }

            // verify that there are now surrogates
            if (!ALLOW_SURROGATES) {
                __m256i equal_ed = _mm256_cmpeq_epi8(_mm256_blendv_epi8(zero, chunk_signed, istate3),
                                                  _mm256_set1_epi8(0xed-0x80));
                if (_mm256_movemask_epi8(equal_ed) != 0) {
                    __m256i mask = _mm256_blendv_epi8(_mm256_set1_epi8(0x80), chunk_signed, _shift_left1_mm256(equal_ed, zero));
                    __m256i check_surrogate = _mm256_cmpgt_epi8(mask, _mm256_set1_epi8(0xa0-1-0x80));
                    if (_mm256_movemask_epi8(check_surrogate) != 0) {
                        // invalid surrograte character!!!
                        return -1;
                    }
                }
            }
        }

        if (_mm256_movemask_epi8(fourbytemarker) != 0) {
            // contain a 4 byte marker
            __m256i istate4 = _shift_left1_mm256(fourbytemarker, zero);
            __m256i iistate4 = _shift_left1_mm256(istate4, zero);
            __m256i state4 =_mm256_or_si256(istate4, iistate4);
            state4 =_mm256_or_si256(state4, _shift_left1_mm256(iistate4, zero));

            contbytes = _mm256_or_si256(contbytes, state4);

            // range check, filter out f0 and 
            __m256i equal_f0 = _mm256_cmpeq_epi8(_mm256_blendv_epi8(zero, chunk_signed, fourbytemarker),
                                              _mm256_set1_epi8(0xf0-0x80));
            if (_mm256_movemask_epi8(equal_f0) != 0) {
                __m256i mask = _mm256_blendv_epi8(_mm256_set1_epi8(0x7f), chunk_signed, _shift_left1_mm256(equal_f0, zero));
                __m256i check_surrogate = _mm256_cmpgt_epi8(_mm256_set1_epi8(0x90-0x80), mask);
                if (_mm256_movemask_epi8(check_surrogate) != 0) {
                    return -1;
                }
            }

            __m256i equal_f4 = _mm256_cmpeq_epi8(_mm256_blendv_epi8(zero, chunk_signed, fourbytemarker),
                                              _mm256_set1_epi8(0xf4-0x80));
            if (_mm256_movemask_epi8(equal_f4) != 0) {
                __m256i mask = _mm256_blendv_epi8(_mm256_set1_epi8(0x80), chunk_signed, _shift_left1_mm256(equal_f4, zero));
                __m256i check_surrogate = _mm256_cmpgt_epi8(mask, _mm256_set1_epi8(0x90-1-0x80));
                if (_mm256_movemask_epi8(check_surrogate) != 0) {
                    return -1;
                }
            }

            __m256i equal_f5_gt = _mm256_cmpgt_epi8(_mm256_blendv_epi8(zero, chunk_signed, fourbytemarker),
                                              _mm256_set1_epi8(0xf4-0x80));
            if (_mm256_movemask_epi8(equal_f5_gt) != 0) {
                return -1;
            }
        }

        // now check that contbytes and the actual byte values have a valid
        // continuation at each position the marker indicates to have one
        __m256i check_cont = _mm256_cmpgt_epi8(zero, contbytes);
        __m256i contpos = _mm256_and_si256(_mm256_set1_epi8(0xc0), chunk);
        contpos = _mm256_cmpeq_epi8(_mm256_set1_epi8(0x80), contpos);
        __m256i validcont = _mm256_xor_si256(check_cont, contpos);
        if (_mm256_movemask_epi8(validcont) != 0) {
            // uff, nope, that is really not utf8
            return -1;
        }

        // CORRECT, calculate the length
        // copy 0x00 over to each place which is a continuation byte
        count = _mm256_blendv_epi8(count, zero, contpos);

        // count the code points using 2x 32 bit hadd and one last 16 hadd
        // the result will end up at the lowest position
        count = _mm256_hadd_epi32(count, zero);
        count = _mm256_hadd_epi32(count, zero);
        count = _mm256_hadd_epi16(count, zero);
        uint16_t c = _mm256_extract_epi16(count, 0);
        uint16_t c2 = _mm256_extract_epi16(count, 8);
        uint16_t points = (c & 0xff) + ((c >> 8) & 0xff) + (c2 & 0xff) + ((c2 >> 8) & 0xff);

        // these cases need to be handled:
        //                      16 byte boundary -> | <- 16 byte boundary
        // -----------------------------------------+--------------------
        // 1) 2 byte code point. e.g. ...  c2       | 80 ...
        // 2) 3 byte code point. e.g. ...  e6       | 80 80 ...
        // 3) 3 byte code point. e.g. ...  e6 80    | 80 ...
        // 4) 4 byte code point. e.g. ...  f2       | 80 80 80 ...
        // 5) 4 byte code point. e.g. ...  f2 80    | 80 80 ...
        // 6) 4 byte code point. e.g. ...  f2 80 80 | 80 ...
        //
        int mask_chunk = _mm256_movemask_epi8(chunk);
        int mask_conti = _mm256_movemask_epi8(contpos);

        // little endian
        int lenoff = 32;
        int minus_codepoints = 0;
        if (BIT(mask_chunk, 32) != 0 && BIT(mask_conti, 32) == 0) { // 1), 2), 4)
            minus_codepoints = 1;
            lenoff -= 1;
        } else if (BIT(mask_chunk, 31) != 0 && BIT(mask_conti, 31) == 0 &&
                   BIT(mask_conti, 32) == 1) { // 3), 5)
            minus_codepoints = 1;
            lenoff -= 2;
        } else if (BIT(mask_chunk, 30) != 0 && BIT(mask_conti, 30) == 0 &&
                   BIT(mask_conti, 31) == 1 && BIT(mask_conti, 32) == 1) { // 6)
            minus_codepoints = 1;
            lenoff -= 3;
        }

        num_codepoints += points - minus_codepoints;
        len -= lenoff;
        encoded += lenoff;
    }

    if (len == 0) {
        return num_codepoints;
    }

    ssize_t result = fu8_count_utf8_codepoints_seq((const char*)encoded, len);
    if (result == -1) {
        return -1;
    }

    return num_codepoints + result;
    return -1;
}

#define REGISTER_BYTES 32

ssize_t _fu8_index_avx2(fu8_idx_lookup_t * l) {
    size_t index = l->codepoint_offset;
    const uint8_t * utf8 = l->utf8 + l->byte_offset;
    const uint8_t * utf8_start_position = l->utf8;
    const uint8_t * utf8_end_position = l->utf8 + l->byte_length;
    int bucket_step = -1;
    int bucket = -1;
    struct fu8_idxtab * itab = l->table[0];

    __builtin_prefetch(utf8, 0, 0);
    if (itab == NULL) {
        l->table[0] = itab = _fu8_alloc_idxtab(l->codepoint_length);
        //printf("table is null allocating\n");
    }

    if (itab) {
        bucket_step = itab->character_step;
        IDX_TO_BUCKET(bucket, l->codepoint_offset, bucket_step);
        //printf("bucket %d step %d iindex_off %ld\n", bucket, bucket_step, l->codepoint_offset);
    }

    if (index == l->codepoint_index) {
        return utf8 - utf8_start_position;
    }

    while (utf8 < utf8_end_position) {
        __m256i chunk = _mm256_loadu_si256((__m256i*)utf8);
        __builtin_prefetch(utf8+REGISTER_BYTES, 0, 0);
        __m256i contmask = _mm256_and_si256(_mm256_set1_epi8(0xc0), chunk);
        __m256i contpos = _mm256_cmpeq_epi8(_mm256_set1_epi8(0x80), contmask);
        int codepointsbits = _mm256_movemask_epi8(contpos);
        int codepoints = REGISTER_BYTES - __builtin_popcount(codepointsbits);

        int mask_chunk = _mm256_movemask_epi8(chunk);
        int mask_conti = _mm256_movemask_epi8(contpos);

        // these cases need to be handled:
        //                      32 byte boundary -> | <- 32 byte boundary
        // -----------------------------------------+--------------------
        // 1) 2 byte code point. e.g. ...  c2       | 80 ...
        // 2) 3 byte code point. e.g. ...  e6       | 80 80 ...
        // 3) 3 byte code point. e.g. ...  e6 80    | 80 ...
        // 4) 4 byte code point. e.g. ...  f2       | 80 80 80 ...
        // 5) 4 byte code point. e.g. ...  f2 80    | 80 80 ...
        // 6) 4 byte code point. e.g. ...  f2 80 80 | 80 ...
        //
        int offset = 32;
        if (BIT(mask_chunk, 32) != 0 && BIT(mask_conti, 32) == 0) { // 1), 2), 4)
            codepoints -= 1;
            offset -= 1;
        } else if (BIT(mask_chunk, 31) != 0 && BIT(mask_conti, 31) == 0 &&
                   BIT(mask_conti, 32) == 1) { // 3), 5)
            codepoints -= 1;
            offset -= 2;
        } else if (BIT(mask_chunk, 30) != 0 && BIT(mask_conti, 30) == 0 &&
                   BIT(mask_conti, 31) == 1 && BIT(mask_conti, 32) == 1) { // 6)
            codepoints -= 1;
            offset -= 3;
        }

        size_t new_index = index + codepoints;

        if (bucket_step != -1 && new_index >= bucket_step && (new_index % bucket_step) <= 32) {
            // the desired byte position is in [offset, offset+16), there is no
            // way to know without walking each byte (only check the MSB of each byte)
            new_index = index;
            for (int p = 0; p < 32; p++) {
                int contib = BIT(mask_conti, p);
                if (!contib) {
                    if ((new_index % bucket_step) == 0) {
                        _fu8_itab_set_bucket(itab, bucket++, utf8 - utf8_start_position + p, new_index);
                        break;
                    }
                    new_index++;
                }
            }
        }

        new_index = index + codepoints;

        if (new_index > l->codepoint_index) {
            // advanced too much, step through
            new_index = index;
            for (int p = 0; p < 32; p++) {
                int contib = BIT(mask_conti, p);
                if (!contib) {
                    if (new_index == l->codepoint_index) {
                        return (utf8 - utf8_start_position) + p;
                    }
                    new_index++;
                }
            }
            assert(0 && "must find codepoint index!");
        }
        utf8 += offset;
        index = new_index;

        if (index == l->codepoint_index) {
            // will most most of the time now directly hit the right code points
            // chances are 1 in 16 bytes...
            return utf8 - utf8_start_position;
        }
    }

    return -1; // out of bounds!!
}
