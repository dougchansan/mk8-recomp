#pragma once
#include <stdint.h>
typedef struct {
    uint64_t x[32], vreg[32][2], fpcr, fpsr;
    int n,z,c,v;
} GuestContext;
#ifdef __cplusplus
extern "C" {
#endif
unsigned differential_count(void);
uint32_t differential_word(unsigned index);
unsigned differential_is_fp(unsigned index);
void differential_emit(unsigned index, GuestContext* state);
#ifdef __cplusplus
}
#endif
