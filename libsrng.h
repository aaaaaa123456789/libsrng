#ifndef ___LIBSRNG

#define ___LIBSRNG

#include <stdint.h>

#ifdef __cplusplus
  // only one declaration, so no braces
  extern "C"
#endif
// parameters:
// state:  pointer to 64-bit RNG state; can't be null
// range:  range of values to generate: a value of 10 will generate values from 0 to 9. If set to 0, the RNG will not
//         be range-limited and it will generate a full 16-bit value. If set to 1, the RNG will return 0 and not
//         generate a value at all - this can be used to reseed a state without consuming a random number.
// reseed: used to generate a new seed for the state; if non-zero, the state will be reseeded this number of times
//         (using a different, and slower, RNG) before generating a random number. This allows for multiple sequences.
uint16_t libsrng_random(uint64_t * state, uint16_t range, unsigned reseed);

#endif
