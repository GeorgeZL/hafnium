/*
 * Copyright 2019 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hf/arch/barriers.h"
#include "hf/arch/types.h"

#include "hf/assert.h"

/* Opaque types for different sized fields of memory mapped IO. */

typedef struct {
	volatile uint8_t *ptr;
} io8_t;

typedef struct {
	volatile uint16_t *ptr;
} io16_t;

typedef struct {
	volatile uint32_t *ptr;
} io32_t;

typedef struct {
	volatile uint64_t *ptr;
} io64_t;

typedef struct {
	volatile uint8_t *base;
	size_t count;
} io8_array_t;

typedef struct {
	volatile uint16_t *base;
	size_t count;
} io16_array_t;

typedef struct {
	volatile uint32_t *base;
	size_t count;
} io32_array_t;

typedef struct {
	volatile uint64_t *base;
	size_t count;
} io64_array_t;

/* Contructors for literals. */

static inline io8_t io8_c(uintpaddr_t addr, uintpaddr_t offset)
{
	return (io8_t){.ptr = (volatile uint8_t *)(addr + offset)};
}

static inline io8_array_t io8_array_c(uintpaddr_t addr, uintpaddr_t offset,
				      uint32_t count)
{
	return (io8_array_t){.base = (volatile uint8_t *)addr, .count = count};
}

static inline io16_t io16_c(uintpaddr_t addr, uintpaddr_t offset)
{
	return (io16_t){.ptr = (volatile uint16_t *)(addr + offset)};
}

static inline io16_array_t io16_array_c(uintpaddr_t addr, uintpaddr_t offset,
					uint32_t count)
{
	return (io16_array_t){.base = (volatile uint16_t *)addr,
			      .count = count};
}

static inline io32_t io32_c(uintpaddr_t addr, uintpaddr_t offset)
{
	return (io32_t){.ptr = (volatile uint32_t *)(addr + offset)};
}

static inline io32_array_t io32_array_c(uintpaddr_t addr, uintpaddr_t offset,
					uint32_t count)
{
	return (io32_array_t){.base = (volatile uint32_t *)addr,
			      .count = count};
}

static inline io64_t io64_c(uintpaddr_t addr, uintpaddr_t offset)
{
	return (io64_t){.ptr = (volatile uint64_t *)(addr + offset)};
}

static inline io64_array_t io64_array_c(uintpaddr_t addr, uintpaddr_t offset,
					uint32_t count)
{
	return (io64_array_t){.base = (volatile uint64_t *)addr,
			      .count = count};
}

#define IO8_C(addr) io8_c((addr), 0)
#define IO16_C(addr) io16_c((addr), 0)
#define IO32_C(addr) io32_c((addr), 0)
#define IO64_C(addr) io64_c((addr), 0)

#define IO8_ARRAY_C(addr, cnt) io8_array_c((addr), 0, cnt)
#define IO16_ARRAY_C(addr, cnt) io16_array_c((addr), 0, cnt)
#define IO32_ARRAY_C(addr, cnt) io32_array_c((addr), 0, cnt)
#define IO64_ARRAY_C(addr, cnt) io64_array_c((addr), 0, cnt)

/** Read from memory-mapped IO. */

static inline uint8_t io_read8(io8_t io)
{
	return *io.ptr;
}

static inline uint16_t io_read16(io16_t io)
{
	return *io.ptr;
}

static inline uint32_t io_read32(io32_t io)
{
	return *io.ptr;
}

static inline uint64_t io_read64(io64_t io)
{
	return *io.ptr;
}

static inline uint8_t io_read8_array(io8_array_t io, size_t n)
{
	assert(n < io.count);
	return io.base[n];
}

static inline uint16_t io_read16_array(io16_array_t io, size_t n)
{
	assert(n < io.count);
	return io.base[n];
}

static inline uint32_t io_read32_array(io32_array_t io, size_t n)
{
	assert(n < io.count);
	return io.base[n];
}

static inline uint64_t io_read64_array(io64_array_t io, size_t n)
{
	assert(n < io.count);
	return io.base[n];
}

/**
 * Read from memory-mapped IO with memory barrier.
 *
 * The read is ordered before subsequent memory accesses.
 */

static inline uint8_t io_read8_mb(io8_t io)
{
	uint8_t v = io_read8(io);

	data_sync_barrier();
	return v;
}

static inline uint16_t io_read16_mb(io16_t io)
{
	uint16_t v = io_read16(io);

	data_sync_barrier();
	return v;
}

static inline uint32_t io_read32_mb(io32_t io)
{
	uint32_t v = io_read32(io);

	data_sync_barrier();
	return v;
}

static inline uint64_t io_read64_mb(io64_t io)
{
	uint64_t v = io_read64(io);

	data_sync_barrier();
	return v;
}

static inline uint8_t io_read8_array_mb(io8_array_t io, size_t n)
{
	uint8_t v = io_read8_array(io, n);

	data_sync_barrier();
	return v;
}

static inline uint16_t io_read16_array_mb(io16_array_t io, size_t n)
{
	uint16_t v = io_read16_array(io, n);

	data_sync_barrier();
	return v;
}

static inline uint32_t io_read32_array_mb(io32_array_t io, size_t n)
{
	uint32_t v = io_read32_array(io, n);

	data_sync_barrier();
	return v;
}

static inline uint64_t io_read64_array_mb(io64_array_t io, size_t n)
{
	uint64_t v = io_read64_array(io, n);

	data_sync_barrier();
	return v;
}

/* Write to memory-mapped IO. */

static inline void io_write8(io8_t io, uint8_t v)
{
	*io.ptr = v;
}

static inline void io_write16(io16_t io, uint16_t v)
{
	*io.ptr = v;
}

static inline void io_write32(io32_t io, uint32_t v)
{
	*io.ptr = v;
}

static inline void io_write64(io64_t io, uint64_t v)
{
	*io.ptr = v;
}

static inline void io_clrbits32(io32_t io, uint32_t clear)
{
	io_write32(io, io_read32(io) & ~clear);
}

static inline void io_setbits32(io32_t io, uint32_t set)
{
	io_write32(io, io_read32(io) | set);
}

static inline void io_clrsetbits32(io32_t io, uint32_t clear, uint32_t set)
{
	io_write32(io, (io_read32(io) & ~clear) | set);
}

static inline void io_write8_array(io8_array_t io, size_t n, uint8_t v)
{
	assert(n < io.count);
	io.base[n] = v;
}

static inline void io_write16_array(io16_array_t io, size_t n, uint16_t v)
{
	assert(n < io.count);
	io.base[n] = v;
}

static inline void io_write32_array(io32_array_t io, size_t n, uint32_t v)
{
	assert(n < io.count);
	io.base[n] = v;
}

static inline void io_write64_array(io64_array_t io, size_t n, uint64_t v)
{
	assert(n < io.count);
	io.base[n] = v;
}

/*
 * Write to memory-mapped IO with memory barrier.
 *
 * The write is ordered after previous memory accesses.
 */

static inline void io_write8_mb(io8_t io, uint8_t v)
{
	data_sync_barrier();
	io_write8(io, v);
}

static inline void io_write16_mb(io16_t io, uint16_t v)
{
	data_sync_barrier();
	io_write16(io, v);
}

static inline void io_write32_mb(io32_t io, uint32_t v)
{
	data_sync_barrier();
	io_write32(io, v);
}

static inline void io_write64_mb(io64_t io, uint64_t v)
{
	data_sync_barrier();
	io_write64(io, v);
}

static inline void io_write8_array_mb(io8_array_t io, size_t n, uint8_t v)
{
	data_sync_barrier();
	io_write8_array(io, n, v);
}

static inline void io_write16_array_mb(io16_array_t io, size_t n, uint16_t v)
{
	data_sync_barrier();
	io_write16_array(io, n, v);
}

static inline void io_write32_array_mb(io32_array_t io, size_t n, uint32_t v)
{
	data_sync_barrier();
	io_write32_array(io, n, v);
}

static inline void io_write64_array_mb(io64_array_t io, size_t n, uint64_t v)
{
	data_sync_barrier();
	io_write64_array(io, n, v);
}

#define __raw_writeb __raw_writeb
static inline void __raw_writeb(uint8_t val, volatile void *addr)
{
	__asm__ volatile("strb %w0, [%1]" : : "r" (val), "r" (addr));
}

#define __raw_writew __raw_writew
static inline void __raw_writew(uint16_t val, volatile void *addr)
{
	__asm__ volatile("strh %w0, [%1]" : : "r" (val), "r" (addr));
}

#define __raw_writel __raw_writel
static inline void __raw_writel(uint32_t val, volatile void *addr)
{
	__asm__ volatile("str %w0, [%1]" : : "r" (val), "r" (addr));
}

#define __raw_writeq __raw_writeq
static inline void __raw_writeq(uint64_t val, volatile void *addr)
{
	__asm__ volatile("str %0, [%1]" : : "r" (val), "r" (addr));
}

#define __raw_readb __raw_readb
static inline uint8_t __raw_readb(const volatile void *addr)
{
	uint8_t val;
	__asm__ volatile("ldrb %w0, [%1]" : "=r" (val) : "r" (addr));
	return val;
}

#define __raw_readw __raw_readw
static inline uint16_t __raw_readw(const volatile void *addr)
{
	uint16_t val;
	__asm__ volatile("ldarh %w0, [%1]" : "=r" (val) : "r" (addr));
	return val;
}

#define __raw_readl __raw_readl
static inline uint32_t __raw_readl(const volatile void *addr)
{
	uint32_t val;
	__asm__ volatile("ldar %w0, [%1]" : "=r" (val) : "r" (addr));
	return val;
}

#define __raw_readq __raw_readq
static inline uint64_t __raw_readq(const volatile void *addr)
{
	uint64_t val;
	__asm__ volatile("ldar %0, [%1]" : "=r" (val) : "r" (addr));
	return val;
}

/*
 * Relaxed I/O memory access primitives. These follow the Device memory
 * ordering rules but do not guarantee any ordering relative to Normal memory
 * accesses.
 */
static inline uint8_t readb_relaxed(uintpaddr_t c)
{
    uint8_t val;

    val = __raw_readb((void *)c);
    iormb();

    return val;
}

static inline uint16_t readw_relaxed(uintpaddr_t c)
{
    uint16_t val;

    val = __raw_readw((void *)c);
    iormb();

    return val;
}

static inline uint32_t readl_relaxed(uintpaddr_t c)
{
    uint32_t val;

    val = __raw_readl((void *)c);
    iormb();

    return val;
}

static inline uint64_t readq_relaxed(uintpaddr_t c)
{
    uint64_t val;

    val = __raw_readq((void *)c);
    iormb();

    return val;
}

static inline void writeb_relaxed(uint8_t v, uintpaddr_t c)
{
    iowmb();
    (void)__raw_writeb(v, (void*)c);
}

static inline void writew_relaxed(uint16_t v, uintpaddr_t c)
{
    iowmb();
    (void)__raw_writew(v, (void *)c);
}

static inline void writel_relaxed(uint32_t v, uintpaddr_t c)
{
    iowmb();
    (void)__raw_writel(v, (void *)c);
}

static inline void writeq_relaxed(uint64_t v, uintpaddr_t c)
{
        iowmb(); 
        (void)__raw_writeq(v, (void *)c);
}
 
#if 0
#define ioread8(addr)		readb_relaxed(addr)
#define ioread16(addr)		readw_relaxed(addr)
#define ioread32(addr)		readl_relaxed(addr)
#define ioread64(addr)		readq_relaxed(addr)

#define iowrite8(v, addr)	writeb_relaxed((v), (addr))
#define iowrite16(v, addr)	writew_relaxed((v), (addr))
#define iowrite32(v, addr)	writel_relaxed((v), (addr))
#define iowrite64(v, addr)	writeq_relaxed((v), (addr))
#else
#define ioread8(addr)		(0)
#define ioread16(addr)		(0)
#define ioread32(addr)		(0)
#define ioread64(addr)		(0)

#define iowrite8(v, addr)	(0)
#define iowrite16(v, addr)	(0)
#define iowrite32(v, addr)	(0)
#define iowrite64(v, addr)	(0)
#endif

#define readb(addr)		readb_relaxed(addr)
#define readw(addr)		readw_relaxed(addr)
#define readl(addr)		readl_relaxed(addr)
#define readq(addr)		readq_relaxed(addr)

#define writeb(v, addr)		writeb_relaxed((v), (addr))
#define writew(v, addr)		writew_relaxed((v), (addr))
#define writel(v, addr)		writel_relaxed((v), (addr))
#define writeq(v, addr)		writeq_relaxed((v), (addr))


