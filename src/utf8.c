#include "utf8-private.h"

#include <stdio.h>
#include <assert.h>

#include <stdlib.h>

#define ISET_SSE4 0x1
#define ISET_AVX2 0x2

#include "utf8-scalar.c" // copy code for scalar operations

int instruction_set = -1;

void detect_instructionset(void)
{
    instruction_set = 0;
    long eax;
    long ebx;
    long ecx;
    long edx;
    long op = 1;

    __asm("cpuid"
        : "=a" (eax),
          "=b" (ebx),
          "=c" (ecx),
          "=d" (edx)
        : "a" (op));

    instruction_set = 0;
    if (ecx & (1<<19)) { // sse4.1
        instruction_set |= ISET_SSE4;
    }
    op = 7;
    __asm("cpuid"
        : "=a" (eax),
          "=b" (ebx),
          "=c" (ecx),
          "=d" (edx)
        : "a" (op));
    if (ebx & (1<<5)) {
        instruction_set |= ISET_AVX2;
    }
}

ssize_t fu8_count_utf8_codepoints(const char * utf8, ssize_t len)
{
    if (instruction_set == -1) {
        detect_instructionset();
    }

    if (len >= 32 && (instruction_set & ISET_AVX2) != 0) {
        // to the MOON!
        return fu8_count_utf8_codepoints_avx(utf8, len);
    }
    if (len >= 16 && (instruction_set == ISET_SSE4) != 0) {
        // speed!!
        return fu8_count_utf8_codepoints_sse4(utf8, len);
    }

    // oh no, just do it sequentially!
    return fu8_count_utf8_codepoints_seq(utf8, len);
}

fu8_idxtab_t * _fu8_alloc_idxtab(int cp_count)
{
    int character_step = 1000;
    if (cp_count <= character_step) {
        return NULL;
    }
    long s = (cp_count/character_step) * sizeof(size_t);
    char * c = calloc(1, sizeof(fu8_idxtab_t)+s);
    fu8_idxtab_t * i = (fu8_idxtab_t*)c;
    i->character_step = character_step;
    i->byte_positions = (size_t*)(c + sizeof(fu8_idxtab_t));
    i->bytepos_table_length = cp_count/character_step;
    return i;
}

void fu8_free_idxtab(struct fu8_idxtab * t)
{
    // why manage this in C?
    // it might at some point have a different data structure,
    // then we can handle this easily here without modifying the API
    free(t); t = NULL;
}

void _fu8_itab_set_bucket(struct fu8_idxtab * tab, int bucket, size_t off, size_t cpidx)
{
    size_t oldval = tab->byte_positions[bucket];
    if (oldval != 0) {
        assert(oldval != off && "table mismatch");
    }
    assert(bucket >= 0 && bucket < tab->bytepos_table_length && "index out of bounds");
    tab->byte_positions[bucket] = off;
    //printf("setting table bucket %d to bytes %d at index %d, %d\n", bucket, off, cpidx, tab->byte_positions[bucket]);
}

ssize_t _fu8_index(fu8_idx_lookup_t * l) {
    // detect instruction set that is available on this machine
    if (instruction_set == -1) { detect_instructionset(); }

    size_t bytelen = l->byte_length;

    if (bytelen >= 32 && (instruction_set & ISET_AVX2) != 0) {
        // to the MOON!
        return _fu8_index_avx2(l);
    }
    if (bytelen >= 16 && (instruction_set & ISET_SSE4) != 0) {
        // some extra speed!!
        return _fu8_index_sse4(l);
    }

    // oh no, just do it sequentially!
    return _fu8_index_seq(l);
}

size_t _fu8_idxtab_lookup_bytepos_i(size_t cpidx, struct fu8_idxtab * tab, size_t * cpoff)
{
    if (cpidx == 0 || tab == NULL) {
        return 0;
    }
    int step = tab->character_step;
    int bucket;
    IDX_TO_BUCKET(bucket, cpidx, step);
    size_t val = tab->byte_positions[bucket];
    //printf("using table bucket %d, *step=%d, %d, idx %d\n", bucket, bucket*step, val, cpidx);
    while (bucket > 0) {
        if (val != 0) {
            *cpoff = (bucket+1) * step;
            //printf("%d at %d ,,, out = %d\n", val, bucket, *cpoff);
            return val;
        }
        bucket--;
        val = tab->byte_positions[bucket];
    }
    // no clue, start at the beginning!
    *cpoff = 0;
    return 0;
}

ssize_t fu8_idx2bytepos(size_t cpidx, size_t cplen,
                        const uint8_t * utf8, size_t utf8len,
                        struct fu8_idxtab ** tab)
{
    if (cpidx <= 0) { return 0; }
    if (cpidx >= cplen) { return -1; }
    size_t cpoff = 0;
    size_t utf8pos = _fu8_idxtab_lookup_bytepos_i(cpidx, tab[0], &cpoff);

    fu8_idx_lookup_t l = {
        .codepoint_index = cpidx,
        .codepoint_offset = cpoff,
        .codepoint_length = cplen,
        .utf8 = utf8,
        .byte_offset = utf8pos,
        .byte_length = utf8len,
        .table = tab
    };

    return _fu8_index(&l);
}
