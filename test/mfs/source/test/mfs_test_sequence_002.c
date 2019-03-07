/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "hal.h"
#include "mfs_test_root.h"

/**
 * @file    mfs_test_sequence_002.c
 * @brief   Test Sequence 002 code.
 *
 * @page mfs_test_sequence_002 [2] Transaction Mode tests
 *
 * File: @ref mfs_test_sequence_002.c
 *
 * <h2>Description</h2>
 * This sequence tests the MFS behavior when used in transaction mode,
 * correct cases and expected error cases are tested.
 *
 * <h2>Test Cases</h2>
 * - @subpage mfs_test_002_001
 * - @subpage mfs_test_002_002
 * .
 */

/****************************************************************************
 * Shared code.
 ****************************************************************************/

#include <string.h>
#include "hal_mfs.h"

static const uint8_t pattern1[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

static const uint8_t pattern2[] = {
  15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

/****************************************************************************
 * Test cases.
 ****************************************************************************/

/**
 * @page mfs_test_002_001 [2.1] Committing a transaction
 *
 * <h2>Description</h2>
 * A set of new/existing records are written/erased within a
 * transaction then the transaction is committed, the state is checked
 * afterward.
 *
 * <h2>Test Steps</h2>
 * - [2.1.1] Records 1, 2 and 3 are created, MFS_NO_ERROR is expected.
 * - [2.1.2] Presence of records 1, 2 and 3 is verified, MFS_NO_ERROR
 *   is expected.
 * - [2.1.3] Starting a transaction with sufficient pre-allocated
 *   space, MFS_NO_ERROR is expected.
 * - [2.1.4] Atomically erasing record 1, updating record 2, reading
 *   record 3.
 * - [2.1.5] Committing the transaction, MFS_NO_ERROR is expected.
 * - [2.1.6] Testing outcome, records 1 must not be present, record 2
 *   must contain the new value and record 3 must be unchanged.
 * - [2.1.7] Re-mounting the manage storage, MFS_NO_ERROR is expected.
 * - [2.1.8] Testing outcome again after re-start.
 * - [2.1.9] Performing a garbage collection, the result must not
 *   change.
 * - [2.1.10] Testing outcome again after garbage collection.
 * .
 */

static void mfs_test_002_001_setup(void) {
  bank_erase(MFS_BANK_0);
  bank_erase(MFS_BANK_1);
  mfsStart(&mfs1, &mfscfg1);
}

static void mfs_test_002_001_teardown(void) {
  mfsStop(&mfs1);
}

static void mfs_test_002_001_execute(void) {
  uint32_t current_counter;
  uint32_t used_space;

  /* [2.1.1] Records 1, 2 and 3 are created, MFS_NO_ERROR is
     expected.*/
  test_set_step(1);
  {
    mfs_error_t err;

    err = mfsWriteRecord(&mfs1, 1, sizeof pattern1, pattern1);
    test_assert(err == MFS_NO_ERROR, "error creating record 1");
    err = mfsWriteRecord(&mfs1, 2, sizeof pattern1, pattern1);
    test_assert(err == MFS_NO_ERROR, "error creating record 2");
    err = mfsWriteRecord(&mfs1, 3, sizeof pattern1, pattern1);
    test_assert(err == MFS_NO_ERROR, "error creating record 3");
  }
  test_end_step(1);

  /* [2.1.2] Presence of records 1, 2 and 3 is verified, MFS_NO_ERROR
     is expected.*/
  test_set_step(2);
  {
    mfs_error_t err;
    size_t size;

    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 1, &size, mfs_buffer);
    test_assert(err == MFS_NO_ERROR, "record not found");
    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 2, &size, mfs_buffer);
    test_assert(err == MFS_NO_ERROR, "record not found");
    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 3, &size, mfs_buffer);
    test_assert(err == MFS_NO_ERROR, "record not found");
  }
  test_end_step(2);

  /* [2.1.3] Starting a transaction with sufficient pre-allocated
     space, MFS_NO_ERROR is expected.*/
  test_set_step(3);
  {
    mfs_error_t err;

    err = mfsStartTransaction(&mfs1, 3U, 1024U);
    test_assert(err == MFS_NO_ERROR, "error starting transaction");
  }
  test_end_step(3);

  /* [2.1.4] Atomically erasing record 1, updating record 2, reading
     record 3.*/
  test_set_step(4);
  {
    mfs_error_t err;
    size_t size;

    err = mfsEraseRecord(&mfs1, 1);
    test_assert(err == MFS_NO_ERROR, "error erasing record 1");
    err = mfsWriteRecord(&mfs1, 2, sizeof pattern2, pattern2);
    test_assert(err == MFS_NO_ERROR, "error writing record 2");
    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 3, &size, mfs_buffer);
    test_assert(err == MFS_NO_ERROR, "record not found");
    test_assert(size == sizeof pattern1, "unexpected record length");
    test_assert(memcmp(pattern1, mfs_buffer, size) == 0, "wrong record content");
  }
  test_end_step(4);

  /* [2.1.5] Committing the transaction, MFS_NO_ERROR is expected.*/
  test_set_step(5);
  {
    mfs_error_t err;

    err = mfsCommitTransaction(&mfs1);
    test_assert(err == MFS_NO_ERROR, "error committing transaction");

    /* Saving some internal state for successive checks.*/
    current_counter = mfs1.current_counter;
    used_space      = mfs1.used_space;
  }
  test_end_step(5);

  /* [2.1.6] Testing outcome, records 1 must not be present, record 2
     must contain the new value and record 3 must be unchanged.*/
  test_set_step(6);
  {
    mfs_error_t err;
    size_t size;

    /* Record 1 must not be present.*/
    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 1, &size, mfs_buffer);
    test_assert(err == MFS_ERR_NOT_FOUND, "record found");

    /* Record 2 must contain the new value.*/
    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 2, &size, mfs_buffer);
    test_assert(err == MFS_NO_ERROR, "record not found");
    test_assert(size == sizeof pattern2, "unexpected record length");
    test_assert(memcmp(pattern2, mfs_buffer, size) == 0, "wrong record content");

    /* Record 3 must be unchanged.*/
    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 3, &size, mfs_buffer);
    test_assert(err == MFS_NO_ERROR, "record not found");
    test_assert(size == sizeof pattern1, "unexpected record length");
    test_assert(memcmp(pattern1, mfs_buffer, size) == 0, "wrong record content");

    /* Checking internal data.*/
    test_assert(MFS_BANK_0 == mfs1.current_bank, "internal data mismatch");
    test_assert(current_counter == mfs1.current_counter, "internal data mismatch");
    test_assert(used_space == mfs1.used_space, "internal data mismatch");
  }
  test_end_step(6);

  /* [2.1.7] Re-mounting the manage storage, MFS_NO_ERROR is
     expected.*/
  test_set_step(7);
  {
    mfs_error_t err;

    err = mfsStart(&mfs1, &mfscfg1);
    test_assert(err == MFS_NO_ERROR, "re-start failed");
  }
  test_end_step(7);

  /* [2.1.8] Testing outcome again after re-start.*/
  test_set_step(8);
  {
    mfs_error_t err;
    size_t size;

    /* Record 1 must not be present.*/
    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 1, &size, mfs_buffer);
    test_assert(err == MFS_ERR_NOT_FOUND, "record found");

    /* Record 2 must contain the new value.*/
    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 2, &size, mfs_buffer);
    test_assert(err == MFS_NO_ERROR, "record not found");
    test_assert(size == sizeof pattern2, "unexpected record length");
    test_assert(memcmp(pattern2, mfs_buffer, size) == 0, "wrong record content");

    /* Record 3 must be unchanged.*/
    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 3, &size, mfs_buffer);
    test_assert(err == MFS_NO_ERROR, "record not found");
    test_assert(size == sizeof pattern1, "unexpected record length");
    test_assert(memcmp(pattern1, mfs_buffer, size) == 0, "wrong record content");

    /* Checking internal data.*/
    test_assert(MFS_BANK_0 == mfs1.current_bank, "internal data mismatch");
    test_assert(current_counter == mfs1.current_counter, "internal data mismatch");
    test_assert(used_space == mfs1.used_space, "internal data mismatch");
  }
  test_end_step(8);

  /* [2.1.9] Performing a garbage collection, the result must not
     change.*/
  test_set_step(9);
  {
    mfs_error_t err;

    err = mfsPerformGarbageCollection(&mfs1);
    test_assert(err == MFS_NO_ERROR, "garbage collection failed");
  }
  test_end_step(9);

  /* [2.1.10] Testing outcome again after garbage collection.*/
  test_set_step(10);
  {
    mfs_error_t err;
    size_t size;

    /* Record 1 must not be present.*/
    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 1, &size, mfs_buffer);
    test_assert(err == MFS_ERR_NOT_FOUND, "record found");

    /* Record 2 must contain the new value.*/
    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 2, &size, mfs_buffer);
    test_assert(err == MFS_NO_ERROR, "record not found");
    test_assert(size == sizeof pattern2, "unexpected record length");
    test_assert(memcmp(pattern2, mfs_buffer, size) == 0, "wrong record content");

    /* Record 3 must be unchanged.*/
    size = sizeof mfs_buffer;
    err = mfsReadRecord(&mfs1, 3, &size, mfs_buffer);
    test_assert(err == MFS_NO_ERROR, "record not found");
    test_assert(size == sizeof pattern1, "unexpected record length");
    test_assert(memcmp(pattern1, mfs_buffer, size) == 0, "wrong record content");

    /* Checking internal data.*/
    test_assert(MFS_BANK_1 == mfs1.current_bank, "internal data mismatch");
    test_assert(current_counter == mfs1.current_counter - 1, "internal data mismatch");
    test_assert(used_space == mfs1.used_space, "internal data mismatch");
  }
  test_end_step(10);
}

static const testcase_t mfs_test_002_001 = {
  "Committing a transaction",
  mfs_test_002_001_setup,
  mfs_test_002_001_teardown,
  mfs_test_002_001_execute
};

/**
 * @page mfs_test_002_002 [2.2] Rolling back a transaction
 *
 * <h2>Description</h2>
 * A set of new/existing records are written/erased within a
 * transaction then the transaction is rolled back, the state is
 * checked afterward.
 *
 * <h2>Test Steps</h2>
 * - [2.2.1] Description.
 * - [2.2.2] Description.
 * - [2.2.3] Description.
 * .
 */

static void mfs_test_002_002_setup(void) {
  bank_erase(MFS_BANK_0);
  bank_erase(MFS_BANK_1);
  mfsStart(&mfs1, &mfscfg1);
}

static void mfs_test_002_002_teardown(void) {
  mfsStop(&mfs1);
}

static void mfs_test_002_002_execute(void) {

  /* [2.2.1] Description.*/
  test_set_step(1);
  {
  }
  test_end_step(1);

  /* [2.2.2] Description.*/
  test_set_step(2);
  {
  }
  test_end_step(2);

  /* [2.2.3] Description.*/
  test_set_step(3);
  {
  }
  test_end_step(3);
}

static const testcase_t mfs_test_002_002 = {
  "Rolling back a transaction",
  mfs_test_002_002_setup,
  mfs_test_002_002_teardown,
  mfs_test_002_002_execute
};

/****************************************************************************
 * Exported data.
 ****************************************************************************/

/**
 * @brief   Array of test cases.
 */
const testcase_t * const mfs_test_sequence_002_array[] = {
  &mfs_test_002_001,
  &mfs_test_002_002,
  NULL
};

/**
 * @brief   Transaction Mode tests.
 */
const testsequence_t mfs_test_sequence_002 = {
  "Transaction Mode tests",
  mfs_test_sequence_002_array
};
