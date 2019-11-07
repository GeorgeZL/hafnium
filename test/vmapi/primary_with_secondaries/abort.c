/*
 * Copyright 2019 The Hafnium Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vmapi/hf/call.h"

#include "hftest.h"
#include "primary_with_secondary.h"
#include "util.h"

/**
 * Accessing unmapped memory aborts the VM.
 */
TEST(abort, data_abort)
{
	struct spci_value run_res;
	struct mailbox_buffers mb = set_up_mailbox();

	SERVICE_SELECT(SERVICE_VM1, "data_abort", mb.send);

	run_res = spci_run(SERVICE_VM1, 0);
	EXPECT_SPCI_ERROR(run_res, SPCI_ABORTED);
}

/**
 * Accessing partially unmapped memory aborts the VM.
 */
TEST(abort, straddling_data_abort)
{
	struct spci_value run_res;
	struct mailbox_buffers mb = set_up_mailbox();

	SERVICE_SELECT(SERVICE_VM1, "straddling_data_abort", mb.send);

	run_res = spci_run(SERVICE_VM1, 0);
	EXPECT_SPCI_ERROR(run_res, SPCI_ABORTED);
}

/**
 * Executing unmapped memory aborts the VM.
 */
TEST(abort, instruction_abort)
{
	struct spci_value run_res;
	struct mailbox_buffers mb = set_up_mailbox();

	SERVICE_SELECT(SERVICE_VM1, "instruction_abort", mb.send);

	run_res = spci_run(SERVICE_VM1, 0);
	EXPECT_SPCI_ERROR(run_res, SPCI_ABORTED);
}

/**
 * Executing partially unmapped memory aborts the VM.
 */
TEST(abort, straddling_instruction_abort)
{
	struct spci_value run_res;
	struct mailbox_buffers mb = set_up_mailbox();

	SERVICE_SELECT(SERVICE_VM1, "straddling_instruction_abort", mb.send);

	run_res = spci_run(SERVICE_VM1, 0);
	EXPECT_SPCI_ERROR(run_res, SPCI_ABORTED);
}
