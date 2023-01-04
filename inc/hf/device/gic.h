/*
 * Copyright 2019 The Hafnium Authors.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/BSD-3-Clause.
 */

#pragma once

#include "hf/mm.h"

int gic_init(void);
int gic_secondary_init(void);
void gic_mm_init(struct mm_stage1_locked stage1_locked, struct mpool *ppool);
