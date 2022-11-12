#pragma once
#include <stddef.h>
#include <stdint.h>

#define BITS_PER_LONG       32
#define BITS_PER_BYTE		8
#define BIT(nr)			    (1UL << (nr))
#define BIT_ULL(nr)		    (1ULL << (nr))
#define BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BIT_ULL_MASK(nr)	(1ULL << ((nr) % BITS_PER_LONG_LONG))
#define BIT_ULL_WORD(nr)	((nr) / BITS_PER_LONG_LONG)
#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(uint32_t))

/*
 * Create a contiguous bitmask starting at bit position @l and ending at
 * position @h. For example
 * GENMASK_ULL(39, 21) gives us the 64bit vector 0x000000ffffe00000.
 */
#define GENMASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define GENMASK_ULL(h, l) \
	(((~0ULL) << (l)) & (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

/*
 * ffs: find first bit set. This is defined the same way as
 * the libc and compiler builtin ffs routines, therefore
 * differs in spirit from the above ffz (man ffs).
 */

static inline uint32_t generic_ffs(uint32_t x)
{
	uint32_t r = 1;

	if (!x)
		return 0;
	if (!(x & 0xffff)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0xff)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0xf)) {
		x >>= 4;
		r += 4;
	}
	if (!(x & 3)) {
		x >>= 2;
		r += 2;
	}
	if (!(x & 1)) {
		x >>= 1;
		r += 1;
	}
	return r;
}

static inline uint32_t generic_ffs_next(uint32_t x, uint32_t pos)
{
    uint32_t mask = ~((1 << pos) - 1);
    uint32_t tmp_x = x & mask;

    return generic_ffs(tmp_x);
}

/**
 * fls - find last (most-significant) bit set
 * @x: the word to search
 *
 * This is defined the same way as ffs.
 * Note fls(0) = 0, fls(1) = 1, fls(0x80000000) = 32.
 */
static inline uint32_t generic_fls(uint32_t x)
{
	uint32_t r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

/* linux/include/asm-generic/bitops/non-atomic.h */

#define set_bit     generic_set_bit
#define clear_bit   generic_clear_bit
#define test_bit    generic_test_bit
#define ffs         generic_ffs
#define ffs_next    generic_ffs_next
#define fls         generic_fls

/**
 * __set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static inline void generic_set_bit(uint32_t nr, volatile uint32_t *addr)
{
	uint32_t mask = BIT_MASK(nr);
	uint32_t *p = ((uint32_t *)addr) + BIT_WORD(nr);

	*p  |= mask;
}

static inline bool generic_test_bit(uint32_t nr, volatile uint32_t *addr)
{
	uint32_t mask = BIT_MASK(nr);
	uint32_t *p = ((uint32_t *)addr) + BIT_WORD(nr);

	return (*p & mask) ? true : false;
}

static inline void generic_clear_bit(uint32_t nr, volatile uint32_t *addr)
{
	uint32_t mask = BIT_MASK(nr);
	uint32_t *p = ((uint32_t *)addr) + BIT_WORD(nr);

	*p &= ~mask;
}
