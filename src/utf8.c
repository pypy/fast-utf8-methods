#include "utf8.h"

#include <stdio.h>

#include "utf8-scalar.c" // copy code for scalar operations


int instruction_set = -1;
#define ISET_SSE4 0x1
#define ISET_AVX 0x2
#define ISET_AVX2 0x4

void detect_instructionset(void)
{
    long eax;
    long ebx;
    long ecx;
    long edx;
    long op = 1;
    asm ("cpuid"
            : "=a" (eax),
              "=b" (ebx),
              "=c" (ecx),
              "=d" (edx)
            : "a" (op));

    instruction_set = 0;
    if (ecx & (1<<19)) { // sse4.1
        instruction_set |= ISET_SSE4;
    }
    if(__builtin_cpu_supports("avx")) {
        instruction_set |= ISET_AVX;
    }
    if(__builtin_cpu_supports("avx2")) {
        instruction_set |= ISET_AVX2;
    }
}

ssize_t count_utf8_codepoints(const uint8_t * encoded, size_t len)
{
    if (instruction_set == -1) {
        detect_instructionset();
    }

    if (len >= 32 && (instruction_set & ISET_AVX2) != 0) {
        // to the MOON!
        return count_utf8_codepoints_avx(encoded, len);
    }
    if (len >= 16 && (instruction_set == ISET_SSE4) != 0) {
        // speed!!
        return count_utf8_codepoints_sse4(encoded, len);
    }

    // oh no, just do it sequentially!
    return count_utf8_codepoints_seq(encoded, len);
}

typedef struct fu8_idxtab {
    volatile int locked;
    uint16_t tab[16];
} fu8_idxtab_t;

#include <stdlib.h>

fu8_idxtab_t * _fu8_alloc_idxtab(int strlen)
{
    if (strlen <= 32) {
        return NULL;
    }
    fu8_idxtab_t * c = (fu8_idxtab_t*)calloc(sizeof(fu8_idxtab_t), 1);
    return c;
}

void fu8_free_idxtab(struct fu8_idxtab * t)
{
    // why manage this in C?
    // it might at some point have a dynamic table inside fu8_idxtab_t,
    // then we can handle this easily here without modifying the API
    free(t); t = NULL;
}

#define FU8_ITAB_STEP(len) len / 16
void _fu8_itab_set_bucket(struct fu8_idxtab * tab, int bucket, size_t len)
{
    assert(bucket < 16 && "bucket must be less than 16");
    assert(bucket >= 0 && "bucket must be a positive integer");
    tab->tab[bucket] = (size_t)len;
}

ssize_t _fu8_build_idxtab(size_t cpidx, const uint8_t * utf8, size_t len,
                          struct fu8_idxtab * tab) {
    size_t code_point_index = 0;
    const uint8_t * utf8_start_position = utf8;
    const uint8_t * utf8_end_position = utf8 + len;
    int bucket_step = FU8_ITAB_STEP(len);
    int bucket = 0;

    while (utf8 < utf8_end_position) {
        if (code_point_index == cpidx) {
            return utf8 - utf8_start_position;
        }

        if (code_point_index != 0 && (code_point_index % bucket_step) == 0) {
            _fu8_itab_set_bucket(tab, bucket++, utf8 - utf8_start_position);
        }

        uint8_t c = *utf8++;
        code_point_index += 1;
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
        if ((c & 0xf7) == 0xf0) {
            utf8 += 3;
            continue;
        }
    }

    return -1; // out of bounds!!
}

ssize_t _fu8_build_idxtab(size_t cpidx, const uint8_t * utf8, size_t len,
                          struct fu8_idxtab * tab) {
    //tab->tab[tab->maxi]
    return tab->tab;
}

ssize_t fu8_idx2bytepos(size_t index,
                        const uint8_t * utf8, size_t len,
                        struct fu8_idxtab ** tab)
{
    struct fu8_idxtab * itab = tab[0];

    if (itab == NULL && len > 16) {
        tab[0] = itab = _fu8_alloc_idxtab(len);
    }
    // note that itab STILL can be NULL

    if (itab == NULL) {
        return _fu8_build_idxtab(index, utf8, len, itab);
    }

    return _fu8_indexed_search(index, utf8, len, itab);
}
