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
    if (strlen <= 1) {
        return NULL;
    }
    return (fu8_idxtab_t*)calloc(sizeof(fu8_idxtab_t), 1);
}

void fu8_free_idxtab(struct fu8_idxtab * t)
{
    // why manage this in C?
    // it might at some point have a dynamic table inside fu8_idxtab_t,
    // then we can handle this easily here without modifying the API
    free(t); t = NULL;
}


ssize_t fu8_idx2bytepos(size_t index,
                        const uint8_t * utf8, size_t len,
                        struct fu8_idxtab ** tab)
{
    size_t code_point_index = 0;
    const uint8_t * utf8_start_position = utf8;
    const uint8_t * utf8_end_position = utf8 + len;

    while (utf8 < utf8_end_position) {
        if (code_point_index == index) {
            return utf8 - utf8_start_position;
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

    return -1;
}
