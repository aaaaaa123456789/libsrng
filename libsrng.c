#include "libsrng.h"

struct libsrng_stable_random_state {
  uint32_t shift;
  uint8_t carry;
  uint8_t current;
  uint8_t prev;
  uint8_t linear;
};

union libsrng_stable_random_state_union {
  uint64_t numeric;
  struct libsrng_stable_random_state structured;
};

static inline uint16_t libsrng_random_linear(uint16_t);
static inline unsigned char libsrng_random_combined(uint64_t *);
static inline uint64_t libsrng_random_combined_multibyte(uint64_t *, unsigned char);
static inline uint16_t libsrng_random_halfword(uint64_t *);
static inline uint64_t libsrng_random_seed(uint64_t *);
static inline unsigned char libsrng_stable_random(struct libsrng_stable_random_state *);
static inline uint16_t libsrng_random_range(uint64_t *, uint16_t);

#define HALFWORD_LCG_MULTIPLIER             0x6329
#define HALFWORD_LCG_ADDEND                 0x4321
#define SEED_LCG_MULTIPLIER     0x5851f42d4c957f2dULL
#define SEED_LCG_FIRST_ADDEND   0x0123456789abcdefULL
#define SEED_LCG_SECOND_ADDEND  0x0fedcba987654321ULL

#define STABLE_RANDOM_NEXT_LINEAR(s) ((s) -> linear *= 73, (s) -> linear += 29, (s) -> linear)

uint16_t libsrng_random (uint64_t * state, uint16_t range, unsigned reseed) {
  if (!state) return 0;
  while (reseed --) *state = libsrng_random_seed(state);
  return libsrng_random_range(state, range);
}

static inline uint16_t libsrng_random_linear (uint16_t previous) {
  return (previous * HALFWORD_LCG_MULTIPLIER + HALFWORD_LCG_ADDEND) & 0xffff;
}

static inline unsigned char libsrng_random_combined (uint64_t * state) {
  // this conditional should be determined at compile time, and it should be true for virtually any reasonable platform
  // compilers should be able to optimize the function without it, but that doesn't seem to be happening
  if (
      // check the sizes of the types involved
      (sizeof *state == sizeof(struct libsrng_stable_random_state)) &&
      (sizeof *state == sizeof(union libsrng_stable_random_state_union)) &&
      // ensure that the machine is little-endian
      (((union libsrng_stable_random_state_union) {.numeric = 0x0123456789abcdef}).structured.shift == 0x89abcdef)
     ) {
    union libsrng_stable_random_state_union * state_union = (void *) state;
    return libsrng_stable_random(&(state_union -> structured));
  } else {
    struct libsrng_stable_random_state temp_state = {.shift = *state, .carry = *state >> 32, .current = *state >> 40,
                                                     .prev = *state >> 48, .linear = *state >> 56};
    unsigned char result = libsrng_stable_random(&temp_state);
    *state = ((uint64_t) temp_state.shift) | ((uint64_t) temp_state.carry << 32) | ((uint64_t) temp_state.current << 40) |
             ((uint64_t) temp_state.prev << 48) | ((uint64_t) temp_state.linear << 56);
    return result;
  }
}

static inline uint64_t libsrng_random_combined_multibyte (uint64_t * state, unsigned char width) {
  if (width > sizeof(uint64_t)) return -1;
  uint64_t result = 0;
  while (width --) result = (result << 8) | libsrng_random_combined(state);
  return result;
}

static inline uint16_t libsrng_random_halfword (uint64_t * state) {
  uint16_t buffer = libsrng_random_combined_multibyte(state, 2);
  unsigned char count = libsrng_random_combined(state);
  unsigned char shift = count >> 4, multiplier = 3 + ((count & 12) >> 1);
  count = (count & 3) + 2;
  while (count --) buffer = libsrng_random_linear(buffer);
  if (shift) buffer = (buffer << shift) | (buffer >> (16 - shift));
  return (buffer * multiplier) & 0xffff;
}

static inline uint64_t libsrng_random_seed (uint64_t * state) {
  uint64_t first = libsrng_random_combined_multibyte(state, 8);
  first = first * SEED_LCG_MULTIPLIER + SEED_LCG_FIRST_ADDEND;
  uint64_t second = 0;
  unsigned count;
  for (count = 0; count < 4; count ++) second = (second << 16) + libsrng_random_halfword(state);
  second = second * SEED_LCG_MULTIPLIER + SEED_LCG_SECOND_ADDEND;
  return first ^ second;
}

static inline unsigned char libsrng_stable_random (struct libsrng_stable_random_state * state) {
  uint32_t p;
  if (!state -> shift) for (p = 0; p < 4; p ++) state -> shift = (state -> shift << 8) | STABLE_RANDOM_NEXT_LINEAR(state);
  state -> shift ^= state -> shift >> 8;
  state -> shift ^= state -> shift << 9;
  state -> shift ^= state -> shift >> 23;
  if (state -> carry >= 210) state -> carry -= 210;
  p = state -> carry + state -> prev + state -> current;
  if (!p || (p == 719)) {
    state -> prev = STABLE_RANDOM_NEXT_LINEAR(state);
    state -> carry = STABLE_RANDOM_NEXT_LINEAR(state);
    state -> current = STABLE_RANDOM_NEXT_LINEAR(state);
  }
  p = 210 * state -> prev + state -> carry;
  state -> prev = state -> current;
  state -> current = p & 0xff;
  state -> carry = p >> 8;
  STABLE_RANDOM_NEXT_LINEAR(state);
  p = state -> shift >> ((state -> linear >> 3) & 24);
  switch ((state -> linear >> 4) & 3) {
    case 0:
      return p + state -> current;
    case 1:
      return p ^ state -> current;
    case 2:
      return p - state -> current;
    default:
      return state -> current - p;
  }
}

static inline uint16_t libsrng_random_range (uint64_t * state, uint16_t limit) {
  if (limit == 1) return 0;
  uint16_t result = libsrng_random_halfword(state);
  if (!(limit & (limit - 1))) return result & (limit - 1);
  if (result >= limit) return result % limit;
  uint16_t resampling_limit = 0x10000 % limit;
  while (result < resampling_limit) result = libsrng_random_halfword(state);
  return result % limit;
}
