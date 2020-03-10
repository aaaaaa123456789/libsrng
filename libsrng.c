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

// the 16-bit generator calls for three 8-bit RNs; this cuts the RNG's period to a third of its maximum
// whenever the RNG lands on one of these values, switch to the next one; this restores the full period
// the constants are just 4.60 fixed point values (rounded to zero) for some fundamental mathematical constants
// (pi, e, the golden ratio) that happen to fall on different RNG cycles
#define SWITCH_TRIGGER_STATE_0  0x3243f6a8885a308dULL
#define SWITCH_TRIGGER_STATE_1  0x2b7e151628aed2a6ULL
#define SWITCH_TRIGGER_STATE_2  0x19e3779b97f4a7c1ULL

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
  const uint64_t switch_trigger_states[] = {SWITCH_TRIGGER_STATE_0, SWITCH_TRIGGER_STATE_1, SWITCH_TRIGGER_STATE_2, SWITCH_TRIGGER_STATE_0};
  unsigned char count;
  for (count = 0; count < (sizeof switch_trigger_states / sizeof *switch_trigger_states - 1); count ++)
    if (*state == switch_trigger_states[count]) {
      *state = switch_trigger_states[count + 1];
      break;
    }
  uint16_t buffer = libsrng_random_combined_multibyte(state, 2);
  count = libsrng_random_combined(state);
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
  const unsigned char cycle_start_points[] = {1, 2, 4, 8, 13, 17, 23, 26, 29, 58, 0};
  const unsigned char short_cycles[] = {0x72, 0x4f, 0x9f, 0x7b, 0x1a, 0x7b, 0x84, 0xe5, 0x56, 0x8d, 0xb0, 0x32, 0, 0, 1};
  if (!state -> shift) for (p = 0; p < 4; p ++) state -> shift = (state -> shift << 8) | STABLE_RANDOM_NEXT_LINEAR(state);
  state -> shift ^= state -> shift >> 8;
  state -> shift ^= state -> shift << 9;
  state -> shift ^= state -> shift >> 23;
  if (state -> prev || state -> current)
    for (p = 0; p < (sizeof short_cycles - 3); p += 3) {
      if ((state -> prev == short_cycles[p]) && (state -> current == short_cycles[p + 1]) && (state -> carry == short_cycles[p + 2])) {
        state -> prev = short_cycles[p + 3];
        state -> current = short_cycles[p + 4];
        state -> carry = short_cycles[p + 5];
        break;
      }
    }
  else
    for (p = 0; p < (sizeof cycle_start_points - 1); p ++) if (state -> carry == cycle_start_points[p]) {
      state -> carry = cycle_start_points[p + 1];
      if (!state -> carry) {
        state -> prev = *short_cycles;
        state -> current = short_cycles[1];
        state -> carry = short_cycles[2];
        STABLE_RANDOM_NEXT_LINEAR(state);
      }
      break;
    }
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
