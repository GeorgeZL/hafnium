/*
 * Copyright 2019 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#pragma once
#include "hf/mpool.h"
#include "hf/vm.h"

#if 0
int vgicv3_init(struct vm *vm, struct mpool *ppool);
int gicv3_init(void);
int gicv3_secondary_init(void);
#endif
int vgic_init(struct vm *vm, struct mpool *ppool);
int gic_init(void);
int gic_secondary_init(void);
