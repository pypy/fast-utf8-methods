#include "utf8-private.h"

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

ssize_t fu8_count_utf8_codepoints_seq(const char * utf8, ssize_t len) {
    size_t num_codepoints = 0;
    uint8_t byte = 0;
    const uint8_t * encoded = (const uint8_t*)utf8;
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

ssize_t _fu8_index_seq(fu8_idx_lookup_t * l) {
    size_t index = l->codepoint_offset;
    const uint8_t * utf8 = l->utf8 + l->byte_offset;
    const uint8_t * utf8_start_position = l->utf8;
    const uint8_t * utf8_end_position = l->utf8 + l->byte_length;

    struct fu8_idxtab * itab = l->table[0];
    if (itab == NULL) {
        l->table[0] = itab = _fu8_alloc_idxtab(l->codepoint_length);
    }

    int bucket_step = -1;
    int bucket = -1;
    if (itab) {
        bucket_step = itab->character_step;
        IDX_TO_BUCKET(bucket, l->codepoint_offset, bucket_step);
        //printf("bucket %d, %d %d\n", bucket, l->codepoint_offset, bucket_step);
    }

    while (utf8 < utf8_end_position) {
        //printf("%d %p %p ok\n", l->codepoint_index, utf8, utf8_start_position);
        if (index == l->codepoint_index) {
            //printf("return %llx %llx %llx, %d\n", utf8_start_position, utf8, utf8_end_position, utf8 - utf8_start_position);
            return utf8 - utf8_start_position;
        }

        if (bucket_step != -1 && index != 0 && (index % bucket_step) == 0) {
            _fu8_itab_set_bucket(itab, bucket++, utf8 - utf8_start_position, index);
        }

        uint8_t c = *utf8++;
        index += 1;
        if ((c & 0xc0) == 0) {
            continue;
        }
        if ((c & 0xe0) == 0xc0) {
            utf8 += 1;
            continue;
        }
        if ((c & 0xf0) == 0xe0) {
            utf8 += 2;
            continue;
        }
        if ((c & 0xf8) == 0xf0) {
            utf8 += 3;
            continue;
        }
    }

    return -1; // out of bounds!!
}
