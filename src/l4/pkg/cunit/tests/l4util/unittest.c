/******************************************************************************
 * Bjoern Doebel <doebel@tudos.org>                                           *
 *                                                                            *
 * (c) 2005 - 2007 Technische Universitaet Dresden                            *
 * This file is part of TUD:OS, which is distributed under the terms of the   *
 * GNU General Public License 2.                                              *
 * Please see the COPYING-GPL-2 file for details.                             *
 ******************************************************************************/

#include <CUnit.h>
#include <Basic.h>
#include <l4/log/macros.h>

#include <l4/util/atomic.h>
#include <l4/util/bitops.h>
#include <l4/util/util.h>

#include <stdio.h>

#ifdef ARCH_x86
#define ALL_TESTS
#endif
#ifdef ARCH_amd64
#define ALL_TESTS
#endif

static int init_testsuite(void)
{
	return 0;
}


static int cleanup_testsuite(void)
{
	return 0;
}


/*****************************************************************************
 * Bit operations tests                                                      *
 *****************************************************************************/

static void test_basic_bitops(void)
{
	l4_umword_t word = 0;
	l4_umword_t words[9] = { 0xffccbbaa, 0xffccbbaa, 0xffccbbaa, 0xffccbbaa,
	                         0xffccbbaa, 0xffccbbaa, 0xffccbbaa, 0xffccbbaa,
	                         0xffccbbaa };
	int i;

	/* Test every bit for initially being off*/
	for (i=0; i < L4_MWORD_BITS; i++)
		CU_ASSERT(l4util_test_bit(i, &word) == 0);

	/* Set each bit, test if it is set, reset it */
	for (i=0; i < L4_MWORD_BITS; i++) {
		l4util_set_bit(i, &word);
		CU_ASSERT(l4util_test_bit(i, &word) == 1);
		l4util_clear_bit(i, &word);
		CU_ASSERT(l4util_test_bit(i, &word) == 0);
	}
	CU_ASSERT(word == 0);

	/* complement_bit() */
	for (i=0; i < L4_MWORD_BITS; i++) {
		l4util_complement_bit(i, &word);
		CU_ASSERT(l4util_test_bit(i, &word) == 1);
		l4util_complement_bit(i, &word);
		CU_ASSERT(l4util_test_bit(i, &word) == 0);
	}

	/* Now go for the field values */
	l4util_clear_bit(245, words);
	CU_ASSERT(words[7] == 0xffccbbaa);
	l4util_clear_bit(227, words);
	CU_ASSERT(words[7] == 0xffccbba2);
}


#ifdef ALL_TESTS
static void test_find_bit(void)
{
	l4_umword_t word = 0;

	/* Testing find_first_bit
	 * 
	 * 1. Everything is set
	 */
	word = ~0;
	CU_ASSERT(l4util_find_first_set_bit(&word, L4_MWORD_BITS) == 0);
	CU_ASSERT(l4util_find_first_zero_bit(&word, L4_MWORD_BITS) >= L4_MWORD_BITS);

	/* 2. everything is zero */
	word = 0;
	CU_ASSERT(l4util_find_first_set_bit(&word, L4_MWORD_BITS) >= L4_MWORD_BITS);
	CU_ASSERT(l4util_find_first_zero_bit(&word, L4_MWORD_BITS) == 0);

	/* 3. first set bit is at bit 5 */
	word = 0;
	l4util_set_bit(5, &word);
	l4util_set_bit(6, &word);
	l4util_set_bit(7, &word);
	l4util_set_bit(8, &word);
	CU_ASSERT(l4util_find_first_set_bit(&word, L4_MWORD_BITS) == 5);

	/* 4. first zero bit is at 4 */
	word = 0;
	l4util_set_bit(0, &word);
	l4util_set_bit(1, &word);
	l4util_set_bit(2, &word);
	l4util_set_bit(3, &word);
	CU_ASSERT(l4util_find_first_zero_bit(&word, L4_MWORD_BITS) == 4);
}
#endif


static void test_and_set(void)
{
	l4_umword_t word = 0;
	int i;

	/* For each bit,
	 *  - test and set
	 *  - test and reset
	 *  - test and complement
	 */
	for (i=0; i < L4_MWORD_BITS; i++) {
		CU_ASSERT(l4util_bts(i, &word) == 0);
		CU_ASSERT(l4util_btr(i, &word) == 1);
#ifdef ALL_TESTS
		CU_ASSERT(l4util_btc(i, &word) == 0);
#else
		word |= 1 << i;
#endif
		CU_ASSERT(l4util_test_bit(i, &word) == 1);
	}
	CU_ASSERT(word == ~0);

	word = 8;
	CU_ASSERT(l4util_test_and_set_bit(2, &word) == 0);
	CU_ASSERT(word == 12);
#ifdef ALL_TESTS
	CU_ASSERT(l4util_test_and_change_bit(0, &word) == 0);
#else
	word |= 1;
#endif
	CU_ASSERT(word == 13);
}


static void test_bit_scan(void)
{
	l4_umword_t word = 0;
	int i;

	/* For (1 << i) both functions will always return i. */
	for (i = 0; i < L4_MWORD_BITS; i++) {
		CU_ASSERT(l4util_bsr(1 << i) == i);
		CU_ASSERT(l4util_bsf(1 << i) == i);
	}

	/* Sampling... */
	word = 0;
	CU_ASSERT(l4util_bsr(word) == -1);
	CU_ASSERT(l4util_bsf(word) == -1);

	word = 0xF;
	CU_ASSERT(l4util_bsr(word) == 3);
	CU_ASSERT(l4util_bsf(word) == 0);

	word = 0xF0;
	CU_ASSERT(l4util_bsr(word) == 7);
	CU_ASSERT(l4util_bsf(word) == 4);

	word = 0xF00;
	CU_ASSERT(l4util_bsr(word) == 11);
	CU_ASSERT(l4util_bsf(word) == 8);

	word = 0xF000;
	CU_ASSERT(l4util_bsr(word) == 15);
	CU_ASSERT(l4util_bsf(word) == 12);

	word = 0x0104;
	CU_ASSERT(l4util_bsr(word) == 8);
	CU_ASSERT(l4util_bsf(word) == 2);

	word = 0x0105;
	CU_ASSERT(l4util_bsr(word) == 8);
	CU_ASSERT(l4util_bsf(word) == 0);
}


/*****************************************************************************
 * compare/exchange tests                                                    *
 *****************************************************************************/

/* There are many cmpxchg variants, and I am too lazy to explicitly write
 * the tests down...
 */

#define test_cmpxchg(bw)	\
	static void test_cmpxchg##bw(void) \
	{ \
		l4_uint##bw##_t val = 103; \
		CU_ASSERT(l4util_cmpxchg##bw(&val, 42, 23) == 0); \
		CU_ASSERT(val == 103); \
		CU_ASSERT(l4util_cmpxchg##bw(&val, 103, 23) != 0); \
		CU_ASSERT(val == 23); \
	}


#define test_xchg(bw)	\
	static void test_xchg##bw(void) \
	{ \
		l4_uint##bw##_t val = 103; \
		CU_ASSERT(l4util_xchg##bw(&val, 23) == 103); \
		CU_ASSERT(val == 23); \
		CU_ASSERT(l4util_xchg##bw(&val, 42) == 23); \
		CU_ASSERT(val == 42); \
	}


/*****************************************************************************
 * add/sub/and/or tests                                                      *
 *****************************************************************************/

#define test_add(bw) \
	static void test_add##bw(void) \
	{ \
		l4_uint##bw##_t val = 42; \
		l4util_add##bw(&val, 23); \
		CU_ASSERT(val == 65); \
	}

#define test_sub(bw) \
	static void test_sub##bw(void) \
	{ \
		l4_uint##bw##_t val = 65; \
		l4util_sub##bw(&val, 23); \
		CU_ASSERT(val == 42); \
	}

#define test_and(bw) \
	static void test_and##bw(void) \
	{ \
		l4_uint##bw##_t val = 0x0F; \
		l4util_and##bw(&val, 0x0A); \
		CU_ASSERT(val == 0x0A); \
		val = 0; \
		l4util_and##bw(&val, 0x23); \
		CU_ASSERT(val == 0); \
	}

#define test_or(bw) \
	static void test_or##bw(void) \
	{ \
		l4_uint##bw##_t val = 0x0F; \
		l4util_or##bw(&val, 0x0A); \
		CU_ASSERT(val == 0x0F); \
		val = 0; \
		l4util_or##bw(&val, 0x23); \
		CU_ASSERT(val == 0x23); \
		val = 0; \
		l4util_or##bw(&val, 0); \
		CU_ASSERT(val == 0); \
	}

#define test_add_res(bw) \
	static void test_add##bw##_res(void) \
	{ \
		l4_uint##bw##_t val = 42; \
		CU_ASSERT(l4util_add##bw##_res(&val, 23) == 65); \
	}

#define test_sub_res(bw) \
	static void test_sub##bw##_res(void) \
	{ \
		l4_uint##bw##_t val = 65; \
		CU_ASSERT(l4util_sub##bw##_res(&val, 23) == 42); \
	}

#define test_and_res(bw) \
	static void test_and##bw##_res(void) \
	{ \
		l4_uint##bw##_t val = 0x0F; \
		CU_ASSERT(l4util_and##bw##_res(&val, 0x0A) == 0x0A); \
		val = 0x0F; \
		CU_ASSERT(l4util_and##bw##_res(&val, 0x3C) == 0x0C); \
		val = 0; \
		CU_ASSERT(l4util_and##bw##_res(&val, 0x23) == 0); \
	}

#define test_or_res(bw) \
	static void test_or##bw##_res(void) \
	{ \
		l4_uint##bw##_t val = 0x0F; \
		CU_ASSERT(l4util_or##bw##_res(&val, 0x0A) == 0x0F); \
		val = 0; \
		CU_ASSERT(l4util_or##bw##_res(&val, 0x23) == 0x23); \
		val = 0; \
		CU_ASSERT(l4util_or##bw##_res(&val, 0) == 0); \
	}


/*****************************************************************************
 * inc/dec tests                                                             *
 *****************************************************************************/

#define test_atomic_inc(bw) \
	static void test_atomic_inc##bw(void) \
	{ \
		l4_uint##bw##_t val = 120; \
		l4util_inc##bw(&val); \
		CU_ASSERT(val == 121); \
		val = ~0; \
		l4util_inc##bw(&val); \
		CU_ASSERT(val == 0); \
	}


#define test_atomic_dec(bw) \
	static void test_atomic_dec##bw(void) \
	{ \
		l4_uint##bw##_t val = 120; \
		l4util_dec##bw(&val); \
		CU_ASSERT(val == 119); \
		val = 0; \
		l4util_dec##bw(&val); \
		CU_ASSERT(val == (l4_uint##bw##_t)~0); \
	}

#define test_atomic_inc_res(bw) \
	static void test_atomic_inc##bw##_res(void) \
	{ \
		l4_uint##bw##_t val = 120; \
		CU_ASSERT(l4util_inc##bw##_res(&val) == 121); \
		val = ~0; \
		CU_ASSERT(l4util_inc##bw##_res(&val) == 0); \
	}


#define test_add_res(bw) \
	static void test_add##bw##_res(void) \
	{ \
		l4_uint##bw##_t val = 42; \
		CU_ASSERT(l4util_add##bw##_res(&val, 23) == 65); \
	}

#define test_sub_res(bw) \
	static void test_sub##bw##_res(void) \
	{ \
		l4_uint##bw##_t val = 65; \
		CU_ASSERT(l4util_sub##bw##_res(&val, 23) == 42); \
	}

#define test_and_res(bw) \
	static void test_and##bw##_res(void) \
	{ \
		l4_uint##bw##_t val = 0x0F; \
		CU_ASSERT(l4util_and##bw##_res(&val, 0x0A) == 0x0A); \
		val = 0x0F; \
		CU_ASSERT(l4util_and##bw##_res(&val, 0x3C) == 0x0C); \
		val = 0; \
		CU_ASSERT(l4util_and##bw##_res(&val, 0x23) == 0); \
	}

#define test_atomic_dec_res(bw) \
	static void test_atomic_dec##bw##_res(void) \
	{ \
		l4_uint##bw##_t val = 120; \
		CU_ASSERT(l4util_dec##bw##_res(&val) == 119); \
		val = 0; \
		CU_ASSERT(l4util_dec##bw##_res(&val) == (l4_uint##bw##_t)~0); \
	}

test_cmpxchg(32)
test_xchg(32)

#ifdef ALL_TESTS
test_cmpxchg(64)
test_add(32)
test_sub(32)
test_and(32)
test_or(32)
test_add_res(32)
test_sub_res(32)
test_and_res(32)
test_or_res(32)
test_atomic_inc(32)
test_atomic_dec(32)
test_atomic_inc_res(32)
test_atomic_dec_res(32)

test_cmpxchg(16)
test_cmpxchg(8)
test_xchg(8)
test_xchg(16)
test_add(8)
test_add(16)
test_sub(8)
test_sub(16)
test_and(8)
test_and(16)
test_or(8)
test_or(16)
test_add_res(8)
test_add_res(16)
test_sub_res(8)
test_sub_res(16)
test_and_res(8)
test_and_res(16)
test_or_res(8)
test_or_res(16)
test_atomic_inc(8)
test_atomic_inc(16)
test_atomic_dec(8)
test_atomic_dec(16)
test_atomic_inc_res(8)
test_atomic_inc_res(16)
test_atomic_dec_res(8)
test_atomic_dec_res(16)
#endif


int main(void)
{
	CU_pSuite bitops_suite = NULL;
	CU_pSuite atomic_suite = NULL;

	int error = CU_initialize_registry();
	if (error != CUE_SUCCESS) {
		printf("Could not initialize CUnit test registry.\n");
		return 1;
	}

	bitops_suite = CU_add_suite("L4Util bitops test", init_testsuite, cleanup_testsuite);
	if (!bitops_suite){
		printf("Error initializing bitops test suite.\n");
		return 1;
	}

	atomic_suite = CU_add_suite("L4Util atomic test", init_testsuite, cleanup_testsuite);
	if (!atomic_suite){
		printf("Error initializing atomic test suite.\n");
		return 1;
	}

	/* Bitops tests */
	CU_ADD_TEST(bitops_suite, test_basic_bitops);
#ifdef ALL_TESTS
	CU_ADD_TEST(bitops_suite, test_find_bit);
#endif
	CU_ADD_TEST(bitops_suite, test_and_set);
	CU_ADD_TEST(bitops_suite, test_bit_scan);

	/* Atomic ops test.
	 * XXX: Note, that these tests do not test he _ATOMIC_ part
	 *      of the function specs. They just check whether the
	 *      operations are carried out correctly.
	 */
	CU_ADD_TEST(atomic_suite, test_cmpxchg32);
	CU_ADD_TEST(atomic_suite, test_xchg32);

#ifdef ALL_TESTS
	CU_ADD_TEST(atomic_suite, test_cmpxchg64);
	CU_ADD_TEST(atomic_suite, test_add32);
	CU_ADD_TEST(atomic_suite, test_and32);
	CU_ADD_TEST(atomic_suite, test_sub32);
	CU_ADD_TEST(atomic_suite, test_or32);
	CU_ADD_TEST(atomic_suite, test_add32_res);
	CU_ADD_TEST(atomic_suite, test_sub32_res);
	CU_ADD_TEST(atomic_suite, test_and32_res);
	CU_ADD_TEST(atomic_suite, test_or32_res);
	CU_ADD_TEST(atomic_suite, test_atomic_inc32);
	CU_ADD_TEST(atomic_suite, test_atomic_dec32);
	CU_ADD_TEST(atomic_suite, test_atomic_inc32_res);
	CU_ADD_TEST(atomic_suite, test_atomic_dec32_res);

	CU_ADD_TEST(atomic_suite, test_cmpxchg8);
	CU_ADD_TEST(atomic_suite, test_cmpxchg16);
	CU_ADD_TEST(atomic_suite, test_xchg8);
	CU_ADD_TEST(atomic_suite, test_xchg16);
	CU_ADD_TEST(atomic_suite, test_add8);
	CU_ADD_TEST(atomic_suite, test_add16);
	CU_ADD_TEST(atomic_suite, test_sub8);
	CU_ADD_TEST(atomic_suite, test_sub16);
	CU_ADD_TEST(atomic_suite, test_and8);
	CU_ADD_TEST(atomic_suite, test_and16);
	CU_ADD_TEST(atomic_suite, test_or8);
	CU_ADD_TEST(atomic_suite, test_or16);
	CU_ADD_TEST(atomic_suite, test_add8_res);
	CU_ADD_TEST(atomic_suite, test_add16_res);
	CU_ADD_TEST(atomic_suite, test_sub8_res);
	CU_ADD_TEST(atomic_suite, test_sub16_res);
	CU_ADD_TEST(atomic_suite, test_and8_res);
	CU_ADD_TEST(atomic_suite, test_and16_res);
	CU_ADD_TEST(atomic_suite, test_or8_res);
	CU_ADD_TEST(atomic_suite, test_or16_res);
	CU_ADD_TEST(atomic_suite, test_atomic_inc8);
	CU_ADD_TEST(atomic_suite, test_atomic_inc16);
	CU_ADD_TEST(atomic_suite, test_atomic_dec8);
	CU_ADD_TEST(atomic_suite, test_atomic_dec16);
	CU_ADD_TEST(atomic_suite, test_atomic_inc8_res);
	CU_ADD_TEST(atomic_suite, test_atomic_inc16_res);
	CU_ADD_TEST(atomic_suite, test_atomic_dec8_res);
	CU_ADD_TEST(atomic_suite, test_atomic_dec16_res);
#endif

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();

	CU_cleanup_registry();

	l4_sleep_forever();

	return 0;
}
