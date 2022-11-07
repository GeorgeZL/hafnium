/*
 * Copyright 2018 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#pragma once

#define stringfy(x)    #x

#if 0
#define read_msr(name)                                              \
	__extension__({                                             \
		uintreg_t __v;                                      \
		__asm__ volatile("mrs %0, " str(name) : "=r"(__v)); \
		__v;                                                \
	})

/**
 * Writes the value to the system register, supported by the current assembler.
 */
#define write_msr(name, value)                                \
	__extension__({                                       \
		__asm__ volatile("msr " str(name) ", %x0"     \
				 :                            \
				 : "rZ"((uintreg_t)(value))); \
	})
#endif

#define read_sysreg32(name)     \
    __extension__ ({						\
        uint32_t _r;							\
        __asm__ volatile("mrs  %0, "stringfy(name) : "=r" (_r));		\
        _r;         \
    })

#define write_sysreg32(v, name)						\
	__extension__({								\
		uint32_t _r = v;					\
		__asm__ volatile("msr "stringfy(name)", %x0"    \
                :               \
                : "rZ"(_r));	\
	})

#define write_sysreg64(v, name)						\
	__extension__({								\
		uint64_t _r = v;					\
		__asm__ volatile("msr "stringfy(name)", %x0"         \
            :               \
            : "rZ" (_r));	\
	})

#define read_sysreg64(name) ({						\
    __extension__({ \
        uint64_t _r;							\
	    __asm__ volatile("mrs  %0, "stringfy(name) : "=r" (_r));		\
	    _r;                     \
    })

#define read_sysreg(name)     //read_sysreg64(name)
#define write_sysreg(v, name) //write_sysreg64(v, name)
