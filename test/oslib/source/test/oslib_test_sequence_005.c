/*
    ChibiOS - Copyright (C) 2006..2017 Giovanni Di Sirio

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
#include "oslib_test_root.h"

/**
 * @file    oslib_test_sequence_005.c
 * @brief   Test Sequence 005 code.
 *
 * @page oslib_test_sequence_005 [5] Memory Pools
 *
 * File: @ref oslib_test_sequence_005.c
 *
 * <h2>Description</h2>
 * This sequence tests the ChibiOS library functionalities related to
 * memory pools.
 *
 * <h2>Conditions</h2>
 * This sequence is only executed if the following preprocessor condition
 * evaluates to true:
 * - CH_CFG_USE_MEMPOOLS
 * .
 *
 * <h2>Test Cases</h2>
 * - @subpage oslib_test_005_001
 * - @subpage oslib_test_005_002
 * - @subpage oslib_test_005_003
 * .
 */

#if (CH_CFG_USE_MEMPOOLS) || defined(__DOXYGEN__)

/****************************************************************************
 * Shared code.
 ****************************************************************************/

#define MEMORY_POOL_SIZE 4

static uint32_t objects[MEMORY_POOL_SIZE];
static MEMORYPOOL_DECL(mp1, sizeof (uint32_t), PORT_NATURAL_ALIGN, NULL);

#if CH_CFG_USE_SEMAPHORES
static GUARDEDMEMORYPOOL_DECL(gmp1, sizeof (uint32_t), PORT_NATURAL_ALIGN);
#endif

static void *null_provider(size_t size, unsigned align) {

  (void)size;
  (void)align;

  return NULL;
}

/****************************************************************************
 * Test cases.
 ****************************************************************************/

#if (CH_CFG_FACTORY_OBJECTS_REGISTRY == TRUE) || defined(__DOXYGEN__)
/**
 * @page oslib_test_005_001 [5.1] Loading and emptying a memory pool
 *
 * <h2>Description</h2>
 * The memory pool functionality is tested by loading and emptying it,
 * all conditions are tested.
 *
 * <h2>Test Steps</h2>
 * - [5.1.1] Adding the objects to the pool using chPoolLoadArray().
 * - [5.1.2] Emptying the pool using chPoolAlloc().
 * - [5.1.3] Now must be empty.
 * - [5.1.4] Adding the objects to the pool using chPoolFree().
 * - [5.1.5] Emptying the pool using chPoolAlloc() again.
 * - [5.1.6] Now must be empty again.
 * - [5.1.7] Covering the case where a provider is unable to return
 *   more memory.
 * .
 */

static void oslib_test_005_001_setup(void) {
  chPoolObjectInit(&mp1, sizeof (uint32_t), NULL);
}

static void oslib_test_005_001_execute(void) {
  unsigned i;

  /* [5.1.1] Adding the objects to the pool using chPoolLoadArray().*/
  test_set_step(1);
  {
    chPoolLoadArray(&mp1, objects, MEMORY_POOL_SIZE);
  }
  test_end_step(1);

  /* [5.1.2] Emptying the pool using chPoolAlloc().*/
  test_set_step(2);
  {
    for (i = 0; i < MEMORY_POOL_SIZE; i++)
      test_assert(chPoolAlloc(&mp1) != NULL, "list empty");
  }
  test_end_step(2);

  /* [5.1.3] Now must be empty.*/
  test_set_step(3);
  {
    test_assert(chPoolAlloc(&mp1) == NULL, "list not empty");
  }
  test_end_step(3);

  /* [5.1.4] Adding the objects to the pool using chPoolFree().*/
  test_set_step(4);
  {
    for (i = 0; i < MEMORY_POOL_SIZE; i++)
      chPoolFree(&mp1, &objects[i]);
  }
  test_end_step(4);

  /* [5.1.5] Emptying the pool using chPoolAlloc() again.*/
  test_set_step(5);
  {
    for (i = 0; i < MEMORY_POOL_SIZE; i++)
      test_assert(chPoolAlloc(&mp1) != NULL, "list empty");
  }
  test_end_step(5);

  /* [5.1.6] Now must be empty again.*/
  test_set_step(6);
  {
    test_assert(chPoolAlloc(&mp1) == NULL, "list not empty");
  }
  test_end_step(6);

  /* [5.1.7] Covering the case where a provider is unable to return
     more memory.*/
  test_set_step(7);
  {
    chPoolObjectInit(&mp1, sizeof (uint32_t), null_provider);
    test_assert(chPoolAlloc(&mp1) == NULL, "provider returned memory");
  }
  test_end_step(7);
}

static const testcase_t oslib_test_005_001 = {
  "Loading and emptying a memory pool",
  oslib_test_005_001_setup,
  NULL,
  oslib_test_005_002_teardown,
  oslib_test_005_002_execute
};
#endif /* CH_CFG_FACTORY_GENERIC_BUFFERS == TRUE */

#if (CH_CFG_USE_SEMAPHORES) || defined(__DOXYGEN__)
/**
 * @page oslib_test_005_002 [5.2] Loading and emptying a guarded memory pool without waiting
 *
 * <h2>Description</h2>
 * The memory pool functionality is tested by loading and emptying it,
 * all conditions are tested.
 *
 * <h2>Conditions</h2>
 * This test is only executed if the following preprocessor condition
 * evaluates to true:
 * - CH_CFG_USE_SEMAPHORES
 * .
 *
 * <h2>Test Steps</h2>
 * - [5.2.1] Adding the objects to the pool using
 *   chGuardedPoolLoadArray().
 * - [5.2.2] Emptying the pool using chGuardedPoolAllocTimeout().
 * - [5.2.3] Now must be empty.
 * - [5.2.4] Adding the objects to the pool using chGuardedPoolFree().
 * - [5.2.5] Emptying the pool using chGuardedPoolAllocTimeout() again.
 * - [5.2.6] Now must be empty again.
 * .
 */

static void oslib_test_005_002_setup(void) {
  chGuardedPoolObjectInit(&gmp1, sizeof (uint32_t));
}

static void oslib_test_005_002_execute(void) {
  unsigned i;

  /* [5.2.1] Adding the objects to the pool using
     chGuardedPoolLoadArray().*/
  test_set_step(1);
  {
    chGuardedPoolLoadArray(&gmp1, objects, MEMORY_POOL_SIZE);
  }
  test_end_step(1);

  /* [5.2.2] Emptying the pool using chGuardedPoolAllocTimeout().*/
  test_set_step(2);
  {
    for (i = 0; i < MEMORY_POOL_SIZE; i++)
      test_assert(chGuardedPoolAllocTimeout(&gmp1, TIME_IMMEDIATE) != NULL, "list empty");
  }
  test_end_step(2);

  /* [5.2.3] Now must be empty.*/
  test_set_step(3);
  {
    test_assert(chGuardedPoolAllocTimeout(&gmp1, TIME_IMMEDIATE) == NULL, "list not empty");
  }
  test_end_step(3);

  /* [5.2.4] Adding the objects to the pool using
     chGuardedPoolFree().*/
  test_set_step(4);
  {
    for (i = 0; i < MEMORY_POOL_SIZE; i++)
      chGuardedPoolFree(&gmp1, &objects[i]);
  }
  test_end_step(4);

  /* [5.2.5] Emptying the pool using chGuardedPoolAllocTimeout()
     again.*/
  test_set_step(5);
  {
    for (i = 0; i < MEMORY_POOL_SIZE; i++)
      test_assert(chGuardedPoolAllocTimeout(&gmp1, TIME_IMMEDIATE) != NULL, "list empty");
  }
  test_end_step(5);

  /* [5.2.6] Now must be empty again.*/
  test_set_step(6);
  {
    test_assert(chGuardedPoolAllocTimeout(&gmp1, TIME_IMMEDIATE) == NULL, "list not empty");
  }
  test_end_step(6);
}

static const testcase_t oslib_test_005_002 = {
  "Loading and emptying a guarded memory pool without waiting",
  oslib_test_005_002_setup,
  NULL,
  oslib_test_005_005_teardown,
  oslib_test_005_005_execute
};
#endif /* CH_CFG_FACTORY_OBJ_FIFOS == TRUE */

#if (CH_CFG_FACTORY_PIPES == TRUE) || defined(__DOXYGEN__)
/**
 * @page oslib_test_005_006 [5.6] Dynamic Pipes Factory
 *
 * <h2>Description</h2>
 * This test case verifies the dynamic pipes factory.
 *
 * <h2>Conditions</h2>
 * This test is only executed if the following preprocessor condition
 * evaluates to true:
 * - CH_CFG_FACTORY_PIPES == TRUE
 * .
 *
 * <h2>Test Steps</h2>
 * - [5.6.1] Retrieving a dynamic pipe by name, must not exist.
 * - [5.6.2] Creating a dynamic pipe it must not exists, must succeed.
 * - [5.6.3] Creating a dynamic pipe with the same name, must fail.
 * - [5.6.4] Retrieving the dynamic pipe by name, must exist, then
 *   increasing the reference counter, finally releasing both
 *   references.
 * - [5.6.5] Releasing the first reference to the dynamic pipe must not
 *   trigger an assertion.
 * - [5.6.6] Retrieving the dynamic pipe by name again, must not exist.
 * .
 */

static void oslib_test_005_006_teardown(void) {
  dyn_pipe_t *dpp;

  dpp = chFactoryFindPipe("mypipe");
  if (dpp != NULL) {
    while (dpp->element.refs > 0U) {
      chFactoryReleasePipe(dpp);
    }
  }
}

static void oslib_test_005_006_execute(void) {
  dyn_pipe_t *dpp;

  /* [5.6.1] Retrieving a dynamic pipe by name, must not exist.*/
  test_set_step(1);
  {
    dpp = chFactoryFindPipe("mypipe");
    test_assert(dpp == NULL, "found");
  }

  /* [5.6.2] Creating a dynamic pipe it must not exists, must
     succeed.*/
  test_set_step(2);
  {
    dpp = chFactoryCreatePipe("mypipe", 16U);
    test_assert(dpp != NULL, "cannot create");
  }

  /* [5.6.3] Creating a dynamic pipe with the same name, must fail.*/
  test_set_step(3);
  {
    dyn_pipe_t *dpp1;

    dpp1 = chFactoryCreatePipe("mypipe", 16U);
    test_assert(dpp1 == NULL, "can create");
  }

  /* [5.6.4] Retrieving the dynamic pipe by name, must exist, then
     increasing the reference counter, finally releasing both
     references.*/
  test_set_step(4);
  {
    dyn_pipe_t *dpp1, *dpp2;

    dpp1 = chFactoryFindPipe("mypipe");
    test_assert(dpp1 != NULL, "not found");
    test_assert(dpp == dpp1, "object reference mismatch");
    test_assert(dpp1->element.refs == 2, "object reference mismatch");

    dpp2 = (dyn_pipe_t *)chFactoryDuplicateReference(&dpp1->element);
    test_assert(dpp1 == dpp2, "object reference mismatch");
    test_assert(dpp2->element.refs == 3, "object reference mismatch");

    chFactoryReleasePipe(dpp2);
    test_assert(dpp1->element.refs == 2, "references mismatch");

    chFactoryReleasePipe(dpp1);
    test_assert(dpp->element.refs == 1, "references mismatch");
  }

  /* [5.6.5] Releasing the first reference to the dynamic pipe must not
     trigger an assertion.*/
  test_set_step(5);
  {
    chFactoryReleasePipe(dpp);
  }

  /* [5.6.6] Retrieving the dynamic pipe by name again, must not
     exist.*/
  test_set_step(6);
  {
    dpp = chFactoryFindPipe("mypipe");
    test_assert(dpp == NULL, "found");
  }
}

static const testcase_t oslib_test_005_006 = {
  "Dynamic Pipes Factory",
  NULL,
  oslib_test_005_006_teardown,
  oslib_test_005_006_execute
};
#endif /* CH_CFG_USE_SEMAPHORES */

#if (CH_CFG_USE_SEMAPHORES) || defined(__DOXYGEN__)
/**
 * @page oslib_test_005_003 [5.3] Guarded Memory Pools timeout
 *
 * <h2>Description</h2>
 * The timeout features for the Guarded Memory Pools is tested.
 *
 * <h2>Conditions</h2>
 * This test is only executed if the following preprocessor condition
 * evaluates to true:
 * - CH_CFG_USE_SEMAPHORES
 * .
 *
 * <h2>Test Steps</h2>
 * - [5.3.1] Trying to allocate with 100mS timeout, must fail because
 *   the pool is empty.
 * .
 */

static void oslib_test_005_003_setup(void) {
  chGuardedPoolObjectInit(&gmp1, sizeof (uint32_t));
}

static void oslib_test_005_003_execute(void) {

  /* [5.3.1] Trying to allocate with 100mS timeout, must fail because
     the pool is empty.*/
  test_set_step(1);
  {
    test_assert(chGuardedPoolAllocTimeout(&gmp1, TIME_MS2I(100)) == NULL, "list not empty");
  }
  test_end_step(1);
}

static const testcase_t oslib_test_005_003 = {
  "Guarded Memory Pools timeout",
  oslib_test_005_003_setup,
  NULL,
  oslib_test_005_003_execute
};
#endif /* CH_CFG_USE_SEMAPHORES */

/****************************************************************************
 * Exported data.
 ****************************************************************************/

/**
 * @brief   Array of test cases.
 */
const testcase_t * const oslib_test_sequence_005_array[] = {
#if (CH_CFG_FACTORY_OBJECTS_REGISTRY == TRUE) || defined(__DOXYGEN__)
  &oslib_test_005_001,
#if (CH_CFG_USE_SEMAPHORES) || defined(__DOXYGEN__)
  &oslib_test_005_002,
#endif
#if (CH_CFG_USE_SEMAPHORES) || defined(__DOXYGEN__)
  &oslib_test_005_003,
#endif
  NULL
};

/**
 * @brief   Memory Pools.
 */
const testsequence_t oslib_test_sequence_005 = {
  "Memory Pools",
  oslib_test_sequence_005_array
};

#endif /* CH_CFG_USE_MEMPOOLS */
