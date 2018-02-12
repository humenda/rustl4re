/*!
 * \file   examples/simple_test/main.c
 * \brief  
 *
 * \date   01/30/2007
 * \author Bjoern Doebel <doebel@os.inf.tu-dresden.de
 *
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <CUnit.h>
#include <Basic.h>
#include <l4/log/macros.h>
#include <l4/util/util.h>

// tested functions
int maxi(int, int);
int mini(int, int);

// test suite
int init_test_suite(void);
int cleanup_test_suite(void);
// tests
void test_maxi(void);
void test_mini(void);

int maxi(int i1, int i2)
{
  return (i1 > i2) ? i1 : i2;
}

int mini(int i, int j)
{
	return maxi(i,j);
}

void test_maxi(void)
{
  CU_ASSERT(maxi(0,2) == 2);
  CU_ASSERT(maxi(0,-2) == 0);
  CU_ASSERT(maxi(2,2) == 2);
}

void test_mini(void)
{
  CU_ASSERT(mini(0,2) == 0);
  CU_ASSERT(mini(0,0) == 0);
  CU_ASSERT(mini(2,-2) == -2);
}

int init_test_suite(void)
{
	LOG("test suite initialization.");
	return 0;
}

int cleanup_test_suite(void)
{
	LOG("test suite cleanup");
	return 0;
}

int main(void)
{
	CU_pSuite suite = NULL;

	int error = CU_initialize_registry();
	if (error == CUE_SUCCESS)
		LOG("Intialized test registry.");
	else
		LOG("Registry initialization failed.");

	suite = CU_add_suite("cunit_simple", init_test_suite, cleanup_test_suite);
	if (suite)
		LOG("Test suite initialized.");
	else {
		CU_ErrorCode e = CU_get_error();
		LOG("Error initializing test suite.");
		LOG("Error was: %d", e);
	}

	CU_ADD_TEST(suite, test_maxi);
	LOG("added test_maxi to suite.");
	CU_ADD_TEST(suite, test_mini);
	LOG("added test_mini to suite.");

	LOG("Running tests in NORMAL mode.");
	CU_basic_set_mode(CU_BRM_NORMAL);
	CU_basic_run_tests();

	LOG("Running tests in SILENT mode.");
	CU_basic_set_mode(CU_BRM_SILENT);
	CU_basic_run_tests();

	LOG("Running tests in VERBOSE mode.");
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();

	CU_cleanup_registry();
	LOG("Registry cleaned up.");

	l4_sleep_forever();

	return 0;
}
