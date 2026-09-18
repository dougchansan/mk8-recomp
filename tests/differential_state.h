#pragma once
/* The differential now runs the real recompiler runtime rather than a stand-in
   context, so the state the oracle and the emitted C share is the production
   GuestContext, generated alongside the translated code by
   `differential-generate --runtime-h`. Memory cases call the production load,
   store, pair and exclusive helpers for real; a private reimplementation of
   those would have tested the harness rather than the recompiler. */
/* Pulled in ahead of the runtime header so the C library declarations it needs
   are not swept into the extern "C" block below, which they do not all
   tolerate. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
/* The runtime is plain C. Without this the runner links against C++-mangled
   names and every helper comes back undefined. */
#include "recomp_runtime.h"
unsigned differential_count(void);
uint32_t differential_word(unsigned index);
/* Cases run one to three instructions. Exclusives need more than one: STXR can
   only succeed after the LDXR that took the reservation, and CLREX is only
   observable through a later STXR, so a single word can never leave the failure
   path. */
unsigned differential_length(unsigned index);
uint32_t differential_word_at(unsigned index, unsigned k);
/* Nonzero for the cases that must touch no memory at all: a store-exclusive
   with no reservation has to fail without storing. Asserted in both directions
   so a case that silently stopped addressing the shared region is caught. */
unsigned differential_expect_no_access(unsigned index);
/* 0 integer, 1 scalar FP, 2 extended FP, 3 memory, 4 memory sequence. */
unsigned differential_is_fp(unsigned index);
void differential_emit(unsigned index, GuestContext* state);
#ifdef __cplusplus
}
#endif
