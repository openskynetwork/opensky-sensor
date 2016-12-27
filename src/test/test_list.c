/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <stdlib.h>
#include "util/list.h"

struct E {
	unsigned int n;
	struct LIST_LinkD listD;
	struct LIST_LinkS listS;
};

static struct LIST_ListDC listD;

static void setupD()
{
	LIST_init(&listD);
}

/** Test (intrusively) the integrity of a double linked counted list
 * @param listD double linked list
 */
static void testDIntegrity(struct LIST_ListDC * listD)
{
	/* the head is the anchor if and only if the tail is the anchor */
	ck_assert_uint_eq((listD->listD.anchor.next == &listD->listD.anchor), (listD->listD.anchor.prev == &listD->listD.anchor));
	/* the head is the anchor if and only if the count is zero or the list is tainted */
	ck_assert_uint_eq((listD->listD.anchor.next == &listD->listD.anchor), (listD->length == 0 || listD->tainted));
	/* the head may never be NULL */
	ck_assert_ptr_ne(listD->listD.anchor.next, NULL);
	/* the tail may never be NULL */
	ck_assert_ptr_ne(listD->listD.anchor.prev, NULL);
	/* the first links' predecessor must be the anchor */
	ck_assert_ptr_eq(listD->listD.anchor.next->prev, &listD->listD.anchor);
	/* the last links' successor must be the anchor */
	ck_assert_ptr_eq(listD->listD.anchor.prev->next, &listD->listD.anchor);

	/* check if each links' predecessor is linked consistently */
	size_t fcnt = 0;
	struct LIST_LinkD * link, *pn;
	for (link = listD->listD.anchor.next, pn = &listD->listD.anchor; link != &listD->listD.anchor; pn = link, link = link->next) {
		ck_assert_ptr_eq(link->prev, pn);
		++fcnt;
	}
	/* also check the forward count on untainted lists */
	if (!listD->tainted)
		ck_assert_uint_eq(listD->length, fcnt);

	/* check if each links' successor is linked consistently */
	size_t bcnt = 0;
	for (link = listD->listD.anchor.prev, pn = &listD->listD.anchor; link != &listD->listD.anchor; pn = link, link = link->prev) {
		ck_assert_ptr_eq(link->next, pn);
		++bcnt;
	}
	/* also check the backward count on untainted lists */
	if (!listD->tainted)
		ck_assert_uint_eq(listD->length, bcnt);

	/* forward and backward count must be equal */
	ck_assert_uint_eq(fcnt, bcnt);
}

START_TEST(test_D_item2link)
{
	struct E e = {5};
	/* check whether LIST_link meets its specification */
	ck_assert_ptr_eq(LIST_link(&e, listD), &e.listD);
}
END_TEST

START_TEST(test_D_link2item)
{
	struct E e = {5};
	struct LIST_LinkD * link = LIST_link(&e, listD);
	/* check whether LIST_item meets its specification */
	ck_assert_ptr_eq(&e, LIST_item(link, struct E, listD));
}
END_TEST

START_TEST(test_D_init_static)
{
	/* test static initializer */
	struct LIST_ListD list = LIST_ListD_INIT(list);
	ck_assert(LIST_empty(&list));
	ck_assert_ptr_ne(LIST_begin(&list), NULL);
	ck_assert_ptr_eq(LIST_begin(&list), LIST_end(&list));
}
END_TEST

START_TEST(test_DC_init_static)
{
	/* test static initializer */
	struct LIST_ListDC list = LIST_ListDC_INIT(list);
	ck_assert(LIST_empty(&list));
	ck_assert_ptr_ne(LIST_begin(&list), NULL);
	ck_assert_ptr_eq(LIST_begin(&list), LIST_end(&list));
	ck_assert_uint_eq(LIST_length(&list), 0);

	testDIntegrity(&list);
}
END_TEST

START_TEST(test_D_init)
{
	/* init is called by the test setup: it must not fail */

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_empty)
{
	/* a fresh list must be empty */
	ck_assert(LIST_empty(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_count0)
{
	/* a fresh list must have length 0 */
	ck_assert_uint_eq(LIST_length(&listD), 0);

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_begin0)
{
	/* the beginning of a fresh list must must be its anchor */
	ck_assert_ptr_eq(LIST_begin(&listD), LIST_end(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_link2itemSafe)
{
	/* LIST_itemSafe must be NULL for the anchor */
	ck_assert_ptr_eq(NULL,
		LIST_itemSafe(&listD, LIST_end(&listD), struct E, listD));

	/* otherwise it must work like LIST_item */
	struct E e;
	struct LIST_LinkD * link = LIST_link(&e, listD);
	ck_assert_ptr_eq(&e, LIST_itemSafe(&listD, link, struct E, listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_push1)
{
	struct E e;
	/* when pushing one link into a fresh list ... */
	LIST_push(&listD, LIST_link(&e, listD));
	/* ... the length must be 1 */
	ck_assert_uint_eq(LIST_length(&listD), 1);
	/* and it must no longer be empty */
	ck_assert(!LIST_empty(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_begin1)
{
	struct E e;
	LIST_push(&listD, LIST_link(&e, listD));

	/* when one link is pushed, LIST_begin must return it */
	struct E * it = LIST_item(LIST_begin(&listD), struct E, listD);
	ck_assert_ptr_eq(&e, it);

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_beginRev1)
{
	struct E e;
	LIST_push(&listD, LIST_link(&e, listD));

	/* when one link is pushed, LIST_beginRev must return it */
	struct E * it = LIST_item(LIST_beginRev(&listD), struct E, listD);
	ck_assert_ptr_eq(&e, it);
	/* and it must not be the anchor */
	ck_assert_ptr_ne(it, LIST_end(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_next1)
{
	struct E e;
	LIST_push(&listD, LIST_link(&e, listD));

	/* when one link is pushed, its successor link ... */
	struct LIST_LinkD * link = LIST_begin(&listD);
	link = LIST_next(link);

	/* ... must be the anchor */
	ck_assert_ptr_eq(link, LIST_end(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_prev1)
{
	struct E e;
	LIST_push(&listD, LIST_link(&e, listD));

	/* when one link is pushed, its predecessor link ... */
	struct LIST_LinkD * link = LIST_begin(&listD);
	link = LIST_prev(link);

	/* ... must be the anchor */
	ck_assert_ptr_eq(link, LIST_end(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_push)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* ... the list length must be 10 */
	ck_assert_uint_eq(LIST_length(&listD), 10);

	/* ... and when iterating with begin/next, all 10 links must be encountered
	 *  in order
	 */
	struct LIST_LinkD * link;
	for (i = 0, link = LIST_begin(&listD); link != LIST_end(&listD) && i < 10; link = LIST_next(link), ++i)
		ck_assert_ptr_eq(&e[i], LIST_item(link, struct E, listD));

	/* it must be all 10 links */
	ck_assert_uint_eq(i, 10);
	/* and the anchor must be encountered last */
	ck_assert_ptr_eq(link, LIST_end(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_iterate_rev)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* ... the list length must be 10 */
	ck_assert_uint_eq(LIST_length(&listD), 10);

	/* ... and when iterating with beginRev/prev, all 10 links must be
	 *  encountered in reverse order
	 */
	struct LIST_LinkD * link;
	for (i = 9, link = LIST_beginRev(&listD); link != LIST_end(&listD); link = LIST_prev(link), --i)
		ck_assert_ptr_eq(&e[i], LIST_item(link, struct E, listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_forward)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* ... and we forward the first link _i times ... */
	struct LIST_LinkD * link = LIST_forward(LIST_begin(&listD), _i);
	/* ... we must encounter the _i-th link */
	ck_assert_ptr_eq(LIST_item(link, struct E, listD), &e[_i]);
}
END_TEST

START_TEST(test_D_forwardEnd)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* ... and we forward the first link 10 times ... */
	struct LIST_LinkD * link = LIST_forward(LIST_begin(&listD), 10);
	/* ... we must encounter the anchor */
	ck_assert_ptr_eq(link, LIST_end(&listD));
}
END_TEST

START_TEST(test_D_rewind)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* ... and we rewind the last link _i times ... */
	struct LIST_LinkD * link = LIST_rewind(LIST_beginRev(&listD), _i);

	/* ... we must encounter the _i-th link (seen from the end) */
	ck_assert_ptr_eq(LIST_item(link, struct E, listD), &e[9 - _i]);
}
END_TEST

START_TEST(test_D_rewindEnd)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* ... and we rewind the last link 10 times ... */
	struct LIST_LinkD * link = LIST_rewind(LIST_beginRev(&listD), 10);
	/* ... we must encounter the anchor */
	ck_assert_ptr_eq(link, LIST_end(&listD));
}
END_TEST

START_TEST(test_D_at)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* ... and we want to have the _i-th link ... */
	struct LIST_LinkD * link = LIST_at(&listD, _i);
	/* ... we expect to get it */
	ck_assert_ptr_eq(LIST_item(link, struct E, listD), &e[_i]);
}
END_TEST

START_TEST(test_D_atEnd)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* ... and we want to have the 10th link ... */
	struct LIST_LinkD * link = LIST_at(&listD, 10);
	/** ... we expect to get the anchor */
	ck_assert_ptr_eq(link, LIST_end(&listD));
}
END_TEST

START_TEST(test_D_pop0)
{
	/* LIST_pop on a fresh list must return the anchor */
	ck_assert_ptr_eq(LIST_pop(&listD), LIST_end(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_push_pop1)
{
	struct E e;
	/* when pushing and popping a link ... */
	LIST_push(&listD, LIST_link(&e, listD));
	/* ... the link must be returned */
	struct E * p = LIST_item(LIST_pop(&listD), struct E, listD);
	ck_assert_ptr_eq(&e, p);

	/* and the list must be empty again */
	ck_assert_uint_eq(LIST_length(&listD), 0);
	ck_assert(LIST_empty(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_push_pop)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	for (i = 10; i >= 1; --i) {
		ck_assert_uint_eq(LIST_length(&listD), i);
		ck_assert(!LIST_empty(&listD));
		/* and we pop one link after another, we expect to encounter the
		 * links in reverse order
		 */
		ck_assert_ptr_eq(LIST_pop(&listD), LIST_link(&e[i - 1], listD));

		testDIntegrity(&listD);
	}
	/* and the list must be empty after popping all links */
	ck_assert_uint_eq(LIST_length(&listD), 0);
	ck_assert(LIST_empty(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_unshift1)
{
	struct E e;
	/* when unshifting one link into a fresh list ... */
	LIST_unshift(&listD, LIST_link(&e, listD));
	/* ... the length must be 1 */
	ck_assert_uint_eq(LIST_length(&listD), 1);
	/* and it must no longer be empty */
	ck_assert(!LIST_empty(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_unshift)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_unshift(&listD, LIST_link(&e[i], listD));

	/* ... the list length must be 10 */
	ck_assert_uint_eq(LIST_length(&listD), 10);

	/* ... and when iterating with begin/next, all 10 links must be encountered
	 *  in reverse order
	 */
	struct LIST_LinkD * link;
	for (i = 10, link = LIST_begin(&listD); link != LIST_end(&listD) && i > 0; link = LIST_next(link), --i)
		ck_assert_ptr_eq(&e[i - 1], LIST_item(link, struct E, listD));

	/* it must be all 10 links */
	ck_assert_uint_eq(i, 0);
	/* and the anchor must be encountered last */
	ck_assert_ptr_eq(link, LIST_end(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_shift0)
{
	/* LIST_shift on a fresh list must return the anchor */
	ck_assert_ptr_eq(LIST_shift(&listD), LIST_end(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_unshift_shift1)
{
	struct E e;
	/* when unshifting and shifting a link ... */
	LIST_unshift(&listD, LIST_link(&e, listD));
	/* ... the link must be returned */
	struct E * p = LIST_item(LIST_shift(&listD), struct E, listD);
	ck_assert_ptr_eq(&e, p);

	/* and the list must be empty again */
	ck_assert_uint_eq(LIST_length(&listD), 0);
	ck_assert(LIST_empty(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_unshift_shift)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are unshifted ... */
	for (i = 0; i < 10; ++i)
		LIST_unshift(&listD, LIST_link(&e[i], listD));

	for (i = 10; i >= 1; --i) {
		ck_assert_uint_eq(LIST_length(&listD), i);
		ck_assert(!LIST_empty(&listD));
		/* and we shift one link after another, we expect to encounter the
		 * links in reverse order
		 */
		ck_assert_ptr_eq(LIST_shift(&listD), LIST_link(&e[i - 1], listD));

		testDIntegrity(&listD);
	}
	/* and the list must be empty after shifting all links */
	ck_assert_uint_eq(LIST_length(&listD), 0);
	ck_assert(LIST_empty(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_unlink0)
{
	/* LIST_unlink on a fresh list with the anchor ... */
	LIST_unlink(&listD, LIST_begin(&listD));

	/* ... the list must still be empty */
	ck_assert(LIST_empty(&listD));
	ck_assert_uint_eq(LIST_length(&listD), 0);

	ck_assert_ptr_eq(LIST_begin(&listD), LIST_end(&listD));
	ck_assert_ptr_eq(LIST_beginRev(&listD), LIST_end(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_unlink)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* ... and we iterate to the _i-th link ... */
	struct LIST_LinkD * link = LIST_at(&listD, _i);
	/* (i.e. not the anchor!) */
	ck_assert_ptr_ne(link, LIST_end(&listD));

	/* ... and unlink it ... */
	LIST_unlink(&listD, link);

	/* then the list length must be decreased by 1 */
	ck_assert_uint_eq(LIST_length(&listD), 9);
	/* and all remaining links must remain in the list, in order */
	for (i = 0, link = LIST_begin(&listD); i < 9; ++i, link = LIST_next(link)) {
		ck_assert_ptr_ne(link, LIST_end(&listD));
		ck_assert_ptr_eq(link, LIST_link(&e[i >= _i ? i + 1 : i], listD));
	}

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_insert)
{
	size_t i;
	struct E e[11];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* ... and we iterate to the _i-th link ... */
	struct LIST_LinkD * link = LIST_at(&listD, _i);
	/* (i.e. not the anchor!) */
	ck_assert_ptr_ne(link, LIST_end(&listD));
	ck_assert_ptr_eq(link, LIST_link(&e[_i], listD));

	/* ... and insert another link here ... */
	LIST_insert(&listD, link, LIST_link(&e[10], listD));

	/* then the list length must be increased by 1 */
	ck_assert_uint_eq(LIST_length(&listD), 11);
	/* and all remaining links must be in the list, in order */
	for (i = 0, link = LIST_begin(&listD); i < 9; ++i, link = LIST_next(link)) {
		ck_assert_ptr_ne(link, LIST_end(&listD));
		ck_assert_ptr_eq(link, LIST_link(&e[i < _i ? i : i == _i ? 10 : i - 1], listD));
	}

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_push_shift)
{
	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	struct LIST_LinkD * link;
	i = 0;
	while ((link = LIST_shift(&listD)) != LIST_end(&listD)) {
		/* ... then shifting all of them must retain the order (queuing) */
		ck_assert_uint_eq(LIST_length(&listD), 9 - i);
		ck_assert_ptr_eq(link, LIST_link(&e[i++], listD));

		testDIntegrity(&listD);
	}

	/* ... and the list must be empty afterwards */
	ck_assert(LIST_empty(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_unshift_pop)
{
	size_t i;
	struct E e[10];

	/* when 10 links are unshifted ... */
	for (i = 0; i < 10; ++i)
		LIST_unshift(&listD, LIST_link(&e[i], listD));

	struct LIST_LinkD * link;
	i = 0;
	while ((link = LIST_pop(&listD)) != LIST_end(&listD)) {
		/* ... then popping all of them must retain the order (queuing) */
		ck_assert_uint_eq(LIST_length(&listD), 9 - i);
		ck_assert_ptr_eq(link, LIST_link(&e[i++], listD));
	}

	/* ... and the list must be empty afterwards */
	ck_assert(LIST_empty(&listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_separateFrom)
{
	struct LIST_ListDC nlist;

	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* ... and we iterate to the _i-th link ... */
	struct LIST_LinkD * link = LIST_at(&listD, _i);
	/* (i.e. not the anchor!) */
	ck_assert_ptr_ne(link, LIST_end(&listD));

	/* and we separate the list from that link */
	LIST_separateFrom(&listD, &nlist, link);

	/* then the old list must include _i links */
	ck_assert_uint_eq(LIST_length(&listD), _i);
	/* and the new one 10-_i */
	ck_assert_uint_eq(LIST_length(&nlist), 10 - _i);

	/* the first _i links must still be in the old list (in order) */
	for (i = 0, link = LIST_begin(&listD); link != LIST_end(&listD); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
	ck_assert_uint_eq(i, _i);

	/* the others links must be in the new list (in order) */
	for (i = _i, link = LIST_begin(&nlist); link != LIST_end(&nlist); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
	ck_assert_uint_eq(i, 10);

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_separateFromEnd)
{
	struct LIST_ListDC nlist;

	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* and we separate the list from the anchor */
	struct LIST_LinkD * link = LIST_end(&listD);
	LIST_separateFrom(&listD, &nlist, link);

	/* then the old list must remain untouched */
	ck_assert_uint_eq(LIST_length(&listD), 10);
	/* and the new list must be initialized */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));

	for (i = 0, link = LIST_begin(&listD); link != LIST_end(&listD); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
	ck_assert_uint_eq(i, 10);

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_separateFromEmpty)
{
	struct LIST_ListDC nlist;

	struct LIST_LinkD * link = LIST_begin(&listD);
	ck_assert_ptr_eq(link, LIST_end(&listD));

	/* when we separate an empty list (from the anchor) */
	LIST_separateFrom(&listD, &nlist, link);

	/* it must remain untouched */
	ck_assert_uint_eq(LIST_length(&listD), 0);
	ck_assert(LIST_empty(&listD));
	/* and the new list must be initialized */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_separateUntil)
{
	struct LIST_ListDC nlist;

	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* ... and we iterate to the _i-th link ... */
	struct LIST_LinkD * link = LIST_at(&listD, _i);
	/* (i.e. not the anchor!) */
	ck_assert_ptr_ne(link, LIST_end(&listD));

	/* and we separate the list up to that link */
	LIST_separateUntil(&listD, &nlist, link);

	/* then the new list must include _i links */
	ck_assert_uint_eq(LIST_length(&nlist), _i);
	/* and the old one 10-_i */
	ck_assert_uint_eq(LIST_length(&listD), 10 - _i);

	/* the first _i links must be in the new list (in order) */
	for (i = 0, link = LIST_begin(&nlist); link != LIST_end(&nlist); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
	ck_assert_uint_eq(i, _i);

	/* the others links must be in the old list (in order) */
	for (i = _i, link = LIST_begin(&listD); link != LIST_end(&listD); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
	ck_assert_uint_eq(i, 10);

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_separateUntilEnd)
{
	struct LIST_ListDC nlist;

	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* and we separate the list up to the anchor */
	struct LIST_LinkD * link = LIST_end(&listD);
	LIST_separateUntil(&listD, &nlist, link);

	/* then the new list must contain all links */
	ck_assert_uint_eq(LIST_length(&nlist), 10);
	/* and the old list must be empty */
	ck_assert_uint_eq(LIST_length(&listD), 0);
	ck_assert(LIST_empty(&listD));

	for (i = 0, link = LIST_begin(&nlist); link != LIST_end(&nlist); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
	ck_assert_uint_eq(i, 10);

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_separateUntilEmpty)
{
	struct LIST_ListDC nlist;

	struct LIST_LinkD * link = LIST_begin(&listD);
	ck_assert_ptr_eq(link, LIST_end(&listD));

	/* when we separate an empty list (from the anchor) */
	LIST_separateUntil(&listD, &nlist, link);

	/* it must remain untouched */
	ck_assert_uint_eq(LIST_length(&listD), 0);
	ck_assert(LIST_empty(&listD));
	/* and the new list must be initialized */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_concatenatePush_toEmpty)
{
	struct LIST_ListDC nlist;

	LIST_init(&nlist);

	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* and the list is concatenated to an empty list */
	LIST_concatenatePush(&nlist, &listD);

	/* the list must be empty */
	ck_assert_uint_eq(LIST_length(&listD), 0);
	ck_assert(LIST_empty(&listD));
	/* and the new list must contain all links in order */
	ck_assert_uint_eq(LIST_length(&nlist), 10);

	struct LIST_LinkD * link;
	for (i = 0, link = LIST_begin(&nlist); link != LIST_end(&nlist); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
	ck_assert_uint_eq(i, 10);

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_concatenatePush_fromEmpty)
{
	struct LIST_ListDC nlist;

	LIST_init(&nlist);

	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* and there is an empty list concatenated */
	LIST_concatenatePush(&listD, &nlist);

	/* the new list must remain empty */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));
	/* and the old list must remain untouched */
	ck_assert_uint_eq(LIST_length(&listD), 10);

	struct LIST_LinkD * link;
	for (i = 0, link = LIST_begin(&listD); link != LIST_end(&listD); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
	ck_assert_uint_eq(i, 10);

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_concatenatePush_twoEmpty)
{
	struct LIST_ListDC nlist;

	LIST_init(&nlist);

	/* when concatenating two empty list */
	LIST_concatenatePush(&listD, &nlist);

	/* both lists must remain empty */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));
	ck_assert_uint_eq(LIST_length(&listD), 0);
	ck_assert(LIST_empty(&listD));

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_concatenatePush)
{
	struct LIST_ListDC nlist;

	LIST_init(&nlist);

	size_t i;
	struct E e[20];

	/* when pushing 10 links to two lists */
	for (i = 0; i < 10; ++i) {
		LIST_push(&listD, LIST_link(&e[i], listD));
		LIST_push(&nlist, LIST_link(&e[i + 10], listD));
	}

	/* and concatenating the lists */
	LIST_concatenatePush(&listD, &nlist);

	/* the source list must be empty */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));
	/* and the destination list must contain all links in order */
	ck_assert_uint_eq(LIST_length(&listD), 20);

	struct LIST_LinkD * link;
	for (i = 0, link = LIST_begin(&listD); link != LIST_end(&listD); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
	ck_assert_uint_eq(i, 20);

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_concatenateUnshift_toEmpty)
{
	struct LIST_ListDC nlist;

	LIST_init(&nlist);

	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* and the list is concatenated to an empty list */
	LIST_concatenateUnshift(&nlist, &listD);

	/* the list must be empty */
	ck_assert_uint_eq(LIST_length(&listD), 0);
	ck_assert(LIST_empty(&listD));
	/* and the new list must contain all links in order */
	ck_assert_uint_eq(LIST_length(&nlist), 10);

	struct LIST_LinkD * link;
	for (i = 0, link = LIST_begin(&nlist); link != LIST_end(&nlist); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
	ck_assert_uint_eq(i, 10);

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_concatenateUnshift_fromEmpty)
{
	struct LIST_ListDC nlist;

	LIST_init(&nlist);

	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listD, LIST_link(&e[i], listD));

	/* and there is an empty list concatenated */
	LIST_concatenateUnshift(&listD, &nlist);

	/* the new list must remain empty */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));
	/* and the old list must remain untouched */
	ck_assert_uint_eq(LIST_length(&listD), 10);

	struct LIST_LinkD * link;
	for (i = 0, link = LIST_begin(&listD); link != LIST_end(&listD); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
	ck_assert_uint_eq(i, 10);

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_concatenateUnshift_twoEmpty)
{
	struct LIST_ListDC nlist;

	LIST_init(&nlist);

	/* when concatenating two empty list */
	LIST_concatenateUnshift(&listD, &nlist);

	/* both lists must remain empty */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));
	ck_assert_uint_eq(LIST_length(&listD), 0);
	ck_assert(LIST_empty(&listD));

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_concatenateUnshift)
{
	struct LIST_ListDC nlist;

	LIST_init(&nlist);

	size_t i;
	struct E e[20];

	/* when pushing 10 links to two lists */
	for (i = 0; i < 10; ++i) {
		LIST_push(&listD, LIST_link(&e[i + 10], listD));
		LIST_push(&nlist, LIST_link(&e[i], listD));
	}

	/* and concatenating the lists */
	LIST_concatenateUnshift(&listD, &nlist);

	/* the source list must be empty */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));
	/* and the destination list must contain all links in order */
	ck_assert_uint_eq(LIST_length(&listD), 20);

	struct LIST_LinkD * link;
	for (i = 0, link = LIST_begin(&listD); link != LIST_end(&listD); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
	ck_assert_uint_eq(i, 20);

	testDIntegrity(&listD);
	testDIntegrity(&nlist);
}
END_TEST

START_TEST(test_D_fromVector)
{
	struct E e[] = { {5}, {6}, {7} };
	/* build list from a vector */
	LIST_fromVector(&listD, e, listD, 3);

	/* list length should be 3 */
	ck_assert_uint_eq(LIST_length(&listD), 3);
	size_t i;
	/* and all links should be in the list in order */
	struct LIST_LinkD * link;
	for (i = 0, link = LIST_begin(&listD); link != LIST_end(&listD); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_fromVectorEmpty)
{
	struct E e[] = { };
	/* build a list from an empty vector */
	LIST_fromVector(&listD, e, listD, 0);

	/* the list should also be empty */
	ck_assert(LIST_empty(&listD));
	ck_assert_uint_eq(LIST_length(&listD), 0);

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_foreach)
{
	struct E e[10];

	/* build a list with 10 links */
	LIST_fromVector(&listD, e, listD, 10);

	/* iterate over the list using a foreach-loop */
	struct LIST_LinkD * link;
	size_t i = 0;
	LIST_foreach(&listD, link)
		ck_assert_ptr_eq(link, LIST_link(&e[i++], listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_foreach_break)
{
	size_t i;
	struct E e[10];

	/* build a list with 10 links */
	for (i = 0; i < 10; ++i) {
		e[i].n = i;
		LIST_push(&listD, LIST_link(&e[i], listD));
	}

	/* iterate over the list using a foreach-loop */
	struct LIST_LinkD * link;
	i = 0;
	LIST_foreach(&listD, link) {
		struct E * it = LIST_item(link, struct E, listD);
		if (it == &e[5]) /* break at the fifth link */
			break;
		/* change the first 5 items */
		it->n += 30;
	}

	/* check whether break has done its work */
	i = 30;
	LIST_foreach(&listD, link) {
		ck_assert_uint_eq(LIST_item(link, struct E, listD)->n, i);
		if (++i == 35)
			i = 5;
	}

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_foreach_unlink)
{
	struct E e[10];

	/* build a list with 10 links */
	LIST_fromVector(&listD, e, listD, 10);

	/* iterate over the list using a foreach-loop */
	struct LIST_LinkD * link, * n;
	LIST_foreachSafe(&listD, link, n) {
		/* unlink the 5th link */
		if (link == LIST_link(&e[5], listD)) {
			LIST_unlink(&listD, link);
			link->next = link->prev = NULL;
		}
	}

	/* list length must be decreased by 1 */
	ck_assert_uint_eq(LIST_length(&listD), 9);

	/* list must be intact without the unlinked link */
	size_t i = 0;
	LIST_foreach(&listD, link) {
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
		if (++i == 5)
			++i;
	}
	ck_assert_uint_eq(i, 10);

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_foreach_rev)
{
	struct E e[10];

	/* build a list with 10 links */
	LIST_fromVector(&listD, e, listD, 10);

	/* iterate backwards over the list using a foreach-loop */
	struct LIST_LinkD * link;
	size_t i = 9;
	LIST_foreachRev(&listD, link)
		ck_assert_ptr_eq(link, LIST_link(&e[i--], listD));

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_foreach_rev_break)
{
	size_t i;
	struct E e[10];

	/* build a list with 10 links */
	for (i = 0; i < 10; ++i) {
		e[i].n = i;
		LIST_unshift(&listD, LIST_link(&e[i], listD));
	}

	/* iterate backwards over the list using a foreach-loop */
	struct LIST_LinkD * link;
	i = 0;
	LIST_foreachRev(&listD, link) {
		struct E * it = LIST_item(link, struct E, listD);
		if (it == &e[5]) /* break at the fifth link */
			break;
		/* change the first 5 items */
		it->n += 30;
	}

	/* check whether break has done its work */
	i = 30;
	LIST_foreachRev(&listD, link) {
		ck_assert_uint_eq(LIST_item(link, struct E, listD)->n, i);
		if (++i == 35)
			i = 5;
	}

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_foreach_rev_unlink)
{
	size_t i;
	struct E e[10];

	/* build a list with 10 links */
	for (i = 0; i < 10; ++i)
		LIST_unshift(&listD, LIST_link(&e[i], listD));

	/* iterate backwards over the list using a foreach-loop */
	struct LIST_LinkD * link, * n;
	LIST_foreachRevSafe(&listD, link, n) {
		/* unlink the 5th link */
		if (link == LIST_link(&e[5], listD)) {
			LIST_unlink(&listD, link);
			link->next = link->prev = NULL;
		}
	}

	/* list length must be decreased by 1 */
	ck_assert_uint_eq(LIST_length(&listD), 9);

	/* list must be intact without the unlinked link */
	i = 0;
	LIST_foreachRev(&listD, link) {
		ck_assert_ptr_eq(link, LIST_link(&e[i], listD));
		if (++i == 5)
			++i;
	}
	ck_assert_uint_eq(i, 10);

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_foreach_item)
{
	struct E e[10];

	/* build a list with 10 links */
	LIST_fromVector(&listD, e, listD, 10);

	/* iterate over the items using a foreach-loop */
	struct E * it;
	size_t i = 0;
	LIST_foreachItem(&listD, it, listD)
		ck_assert_ptr_eq(it, &e[i++]);

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_foreach_item_cont)
{
	struct E e[10];

	/* build a list with 10 links */
	LIST_fromVector(&listD, e, listD, 10);

	/* get the _i-th elements */
	struct E * it = LIST_item(LIST_at(&listD, _i), struct E, listD);
	/* iterate over the items from there using a foreach-loop */
	size_t i = _i + 1;
	LIST_foreachItem_continue(&listD, it, listD)
		ck_assert_ptr_eq(it, &e[i++]);

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_foreach_item_rev)
{
	struct E e[10];

	/* build a list with 10 links */
	LIST_fromVector(&listD, e, listD, 10);

	/* iterate backwards over the items using a foreach-loop */
	struct E * it;
	size_t i = 9;
	LIST_foreachItemRev(&listD, it, listD)
		ck_assert_ptr_eq(it, &e[i--]);

	testDIntegrity(&listD);
}
END_TEST

START_TEST(test_D_foreach_item_rev_cont)
{
	struct E e[10];

	/* build a list with 10 links */
	LIST_fromVector(&listD, e, listD, 10);

	/* get the _i-th elements */
	struct E * it = LIST_item(LIST_at(&listD, _i), struct E, listD);
	/* iterate backwards over the items from there using a foreach-loop */
	size_t i = _i - 1;
	LIST_foreachItemRev_continue(&listD, it, listD)
		ck_assert_ptr_eq(it, &e[i--]);

	testDIntegrity(&listD);
}
END_TEST


static struct LIST_ListSC listS;

static void setupS()
{
	LIST_init(&listS);
}

/** Test (intrusively) the integrity of a double linked counted list
 * @param listD double linked list
 */
static void testSIntegrity(struct LIST_ListSC * listS)
{
	/* the head is the anchor if and only if the tail is the anchor */
	ck_assert_uint_eq((listS->listS.anchor.next == &listS->listS.anchor), (listS->listS.tail == &listS->listS.anchor));
	/* the head is the anchor if and only if the count is zero or the list is tainted */
	ck_assert_uint_eq((listS->listS.anchor.next == &listS->listS.anchor), (listS->length == 0));
	/* the head may never be NULL */
	ck_assert_ptr_ne(listS->listS.anchor.next, NULL);
	/* the tail may never be NULL */
	ck_assert_ptr_ne(listS->listS.tail, NULL);

	/* check the count */
	size_t cnt = 0;
	struct LIST_LinkS * link;
	for (link = listS->listS.anchor.next; link != &listS->listS.anchor; link = link->next)
		++cnt;

	ck_assert_uint_eq(listS->length, cnt);
}

START_TEST(test_S_item2link)
{
	struct E e = {5};
	/* check whether LIST_link meets its specification */
	ck_assert_ptr_eq(LIST_link(&e, listS), &e.listS);
}
END_TEST

START_TEST(test_S_link2item)
{
	struct E e = {5};
	struct LIST_LinkS * link = LIST_link(&e, listS);
	/* check whether LIST_item meets its specification */
	ck_assert_ptr_eq(&e, LIST_item(link, struct E, listS));
}
END_TEST

START_TEST(test_S_init_static)
{
	/* test static initializer */
	struct LIST_ListS list = LIST_ListS_INIT(list);
	ck_assert(LIST_empty(&list));
	ck_assert_ptr_ne(LIST_begin(&list), NULL);
	ck_assert_ptr_eq(LIST_begin(&list), LIST_end(&list));
}
END_TEST

START_TEST(test_SC_init_static)
{
	/* test static initializer */
	struct LIST_ListSC list = LIST_ListSC_INIT(list);
	ck_assert(LIST_empty(&list));
	ck_assert_ptr_ne(LIST_begin(&list), NULL);
	ck_assert_ptr_eq(LIST_begin(&list), LIST_end(&list));
	ck_assert_uint_eq(LIST_length(&list), 0);

	testSIntegrity(&list);
}
END_TEST

START_TEST(test_S_init)
{
	/* init is called by the test setup: it must not fail */

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_empty)
{
	/* a freshly initialized list must be empty */
	ck_assert(LIST_empty(&listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_count0)
{
	/* a fresh list must have length 0 */
	ck_assert_uint_eq(LIST_length(&listS), 0);

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_begin0)
{
	/* the beginning of a fresh list must must be its anchor */
	ck_assert_ptr_eq(LIST_begin(&listS), LIST_end(&listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_link2itemSafe)
{
	/* LIST_itemSafe must be NULL for the anchor */
	ck_assert_ptr_eq(NULL,
		LIST_itemSafe(&listS, LIST_begin(&listS), struct E, listS));

	/* otherwise it must work like LIST_item */
	struct E e;
	struct LIST_LinkS * link = LIST_link(&e, listS);
	ck_assert_ptr_eq(&e, LIST_itemSafe(&listS, link, struct E, listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_push1)
{
	struct E e;
	/* when pushing one link into a fresh list ... */
	LIST_push(&listS, LIST_link(&e, listS));
	/* ... the length must be 1 */
	ck_assert_uint_eq(LIST_length(&listS), 1);
	/* and it must no longer be empty */
	ck_assert(!LIST_empty(&listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_begin1)
{
	struct E e;
	LIST_push(&listS, LIST_link(&e, listS));

	/* when one link is pushed, LIST_begin must return it */
	struct E * it = LIST_item(LIST_begin(&listS), struct E, listS);
	ck_assert_ptr_eq(&e, it);

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_next1)
{
	struct E e;
	LIST_push(&listS, LIST_link(&e, listS));

	/* when one link is pushed, its successor link ... */
	struct LIST_LinkS * link = LIST_begin(&listS);
	link = LIST_next(link);

	/* ... must be the anchor */
	ck_assert_ptr_eq(link, LIST_end(&listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_push)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listS, LIST_link(&e[i], listS));

	/* ... the list length must be 10 */
	ck_assert_uint_eq(LIST_length(&listS), 10);

	/* ... and when iterating with begin/next, all 10 links must be encountered
	 *  in order
	 */
	struct LIST_LinkS * link;
	for (i = 0, link = LIST_begin(&listS); link != LIST_end(&listS) && i < 10; link = LIST_next(link), ++i)
		ck_assert_ptr_eq(&e[i], LIST_item(link, struct E, listS));

	/* it must be all 10 links */
	ck_assert_uint_eq(i, 10);
	/* and the anchor must be encountered last */
	ck_assert_ptr_eq(link, LIST_end(&listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_forward)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listS, LIST_link(&e[i], listS));

	/* ... and we forward the first link _i times ... */
	struct LIST_LinkS * link = LIST_forward(LIST_begin(&listS), _i);
	/* ... we must encounter the _i-th link */
	ck_assert_ptr_eq(LIST_item(link, struct E, listS), &e[_i]);
}
END_TEST

START_TEST(test_S_forwardEnd)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listS, LIST_link(&e[i], listS));

	/* ... and we forward the first link 10 times ... */
	struct LIST_LinkS * link = LIST_forward(LIST_begin(&listS), 10);
	/* ... we must encounter the anchor */
	ck_assert_ptr_eq(link, LIST_end(&listS));
}
END_TEST

START_TEST(test_S_at)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listS, LIST_link(&e[i], listS));

	/* ... and we want to have the _i-th link ... */
	struct LIST_LinkS * link = LIST_at(&listS, _i);
	/* ... we expect to get it */
	ck_assert_ptr_eq(LIST_item(link, struct E, listS), &e[_i]);
}
END_TEST

START_TEST(test_S_atEnd)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listS, LIST_link(&e[i], listS));

	/* ... and we want to have the 10th link ... */
	struct LIST_LinkS * link = LIST_at(&listS, 10);
	/** ... we expect to get the anchor */
	ck_assert_ptr_eq(link, LIST_end(&listS));
}
END_TEST

START_TEST(test_S_unshift1)
{
	struct E e;
	/* when unshifting one link into a fresh list ... */
	LIST_unshift(&listS, LIST_link(&e, listS));
	/* ... the length must be 1 */
	ck_assert_uint_eq(LIST_length(&listS), 1);
	/* and it must no longer be empty */
	ck_assert(!LIST_empty(&listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_unshift)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_unshift(&listS, LIST_link(&e[i], listS));

	/* ... the list length must be 10 */
	ck_assert_uint_eq(LIST_length(&listS), 10);

	/* ... and when iterating with begin/next, all 10 links must be encountered
	 *  in reverse order
	 */
	struct LIST_LinkS * link;
	for (i = 10, link = LIST_begin(&listS); link != LIST_end(&listS) && i > 0; link = LIST_next(link), --i)
		ck_assert_ptr_eq(&e[i - 1], LIST_item(link, struct E, listS));

	/* it must be all 10 links */
	ck_assert_uint_eq(i, 0);
	/* and the anchor must be encountered last */
	ck_assert_ptr_eq(link, LIST_end(&listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_shift0)
{
	/* LIST_shift on a fresh list must return the anchor */
	ck_assert_ptr_eq(LIST_shift(&listS), LIST_end(&listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_unshift_shift1)
{
	struct E e;
	/* when unshifting and shifting a link ... */
	LIST_unshift(&listS, LIST_link(&e, listS));
	/* ... the link must be returned */
	struct E * p = LIST_item(LIST_shift(&listS), struct E, listS);
	ck_assert_ptr_eq(&e, p);

	/* and the list must be empty again */
	ck_assert_uint_eq(LIST_length(&listS), 0);
	ck_assert(LIST_empty(&listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_unshift_shift)
{
	size_t i;
	struct E e[10];

	/* when 10 elements are unshifted ... */
	for (i = 0; i < 10; ++i)
		LIST_unshift(&listS, LIST_link(&e[i], listS));

	for (i = 10; i >= 1; --i) {
		ck_assert_uint_eq(LIST_length(&listS), i);
		ck_assert(!LIST_empty(&listS));
		/* and we shift one link after another, we expect to encounter the
		 * links in reverse order
		 */
		ck_assert_ptr_eq(LIST_shift(&listS), LIST_link(&e[i - 1], listS));

		testSIntegrity(&listS);
	}
	/* and the list must be empty after shifting all links */
	ck_assert_uint_eq(LIST_length(&listS), 0);
	ck_assert(LIST_empty(&listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_push_shift)
{
	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listS, LIST_link(&e[i], listS));

	struct LIST_LinkS * link;
	i = 0;
	while ((link = LIST_shift(&listS)) != LIST_end(&listS)) {
		/* ... then shifting all of them must retain the order (queuing) */
		ck_assert_uint_eq(LIST_length(&listS), 9 - i);
		ck_assert_ptr_eq(link, LIST_link(&e[i++], listS));
	}

	/* ... and the list must be empty afterwards */
	ck_assert(LIST_empty(&listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_concatenatePush_toEmpty)
{
	struct LIST_ListSC nlist;

	LIST_init(&nlist);

	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listS, LIST_link(&e[i], listS));

	/* and the list is concatenated to an empty list */
	LIST_concatenatePush(&nlist, &listS);

	/* the list must be empty */
	ck_assert_uint_eq(LIST_length(&listS), 0);
	ck_assert(LIST_empty(&listS));
	/* and the new list must contain all links in order */
	ck_assert_uint_eq(LIST_length(&nlist), 10);

	struct LIST_LinkS * link;
	for (i = 0, link = LIST_begin(&nlist); link != LIST_end(&nlist); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listS));
	ck_assert_uint_eq(i, 10);

	testSIntegrity(&listS);
	testSIntegrity(&nlist);
}
END_TEST

START_TEST(test_S_concatenatePush_fromEmpty)
{
	struct LIST_ListSC nlist;

	LIST_init(&nlist);

	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listS, LIST_link(&e[i], listS));

	/* and there is an empty list concatenated */
	LIST_concatenatePush(&listS, &nlist);

	/* the new list must remain empty */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));
	/* and the old list must remain untouched */
	ck_assert_uint_eq(LIST_length(&listS), 10);

	struct LIST_LinkS * link;
	for (i = 0, link = LIST_begin(&listS); link != LIST_end(&listS); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listS));
	ck_assert_uint_eq(i, 10);

	testSIntegrity(&listS);
	testSIntegrity(&nlist);
}
END_TEST

START_TEST(test_S_concatenatePush_twoEmpty)
{
	struct LIST_ListSC nlist;

	LIST_init(&nlist);

	/* when concatenating two empty list */
	LIST_concatenatePush(&listS, &nlist);

	/* both lists must remain empty */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));
	ck_assert_uint_eq(LIST_length(&listS), 0);
	ck_assert(LIST_empty(&listS));

	testSIntegrity(&listS);
	testSIntegrity(&nlist);
}
END_TEST

START_TEST(test_S_concatenatePush)
{
	struct LIST_ListSC nlist;

	LIST_init(&nlist);

	size_t i;
	struct E e[20];

	/* when pushing 10 links to two lists */
	for (i = 0; i < 10; ++i) {
		LIST_push(&listS, LIST_link(&e[i], listS));
		LIST_push(&nlist, LIST_link(&e[i + 10], listS));
	}

	/* and concatenating the lists */
	LIST_concatenatePush(&listS, &nlist);

	/* the source list must be empty */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));
	/* and the destination list must contain all links in order */
	ck_assert_uint_eq(LIST_length(&listS), 20);

	struct LIST_LinkS * link;
	for (i = 0, link = LIST_begin(&listS); link != LIST_end(&listS); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listS));
	ck_assert_uint_eq(i, 20);

	testSIntegrity(&listS);
	testSIntegrity(&nlist);
}
END_TEST

START_TEST(test_S_concatenateUnshift_toEmpty)
{
	struct LIST_ListSC nlist;

	LIST_init(&nlist);

	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listS, LIST_link(&e[i], listS));

	/* and the list is concatenated to an empty list */
	LIST_concatenateUnshift(&nlist, &listS);

	/* the list must be empty */
	ck_assert_uint_eq(LIST_length(&listS), 0);
	ck_assert(LIST_empty(&listS));
	/* and the new list must contain all links in order */
	ck_assert_uint_eq(LIST_length(&nlist), 10);

	struct LIST_LinkS * link;
	for (i = 0, link = LIST_begin(&nlist); link != LIST_end(&nlist); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listS));
	ck_assert_uint_eq(i, 10);

	testSIntegrity(&listS);
	testSIntegrity(&nlist);
}
END_TEST

START_TEST(test_S_concatenateUnshift_fromEmpty)
{
	struct LIST_ListSC nlist;

	LIST_init(&nlist);

	size_t i;
	struct E e[10];

	/* when 10 links are pushed ... */
	for (i = 0; i < 10; ++i)
		LIST_push(&listS, LIST_link(&e[i], listS));

	/* and there is an empty list concatenated */
	LIST_concatenateUnshift(&listS, &nlist);

	/* the new list must remain empty */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));
	/* and the old list must remain untouched */
	ck_assert_uint_eq(LIST_length(&listS), 10);

	struct LIST_LinkS * link;
	for (i = 0, link = LIST_begin(&listS); link != LIST_end(&listS); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listS));
	ck_assert_uint_eq(i, 10);

	testSIntegrity(&listS);
	testSIntegrity(&nlist);
}
END_TEST

START_TEST(test_S_concatenateUnshift_twoEmpty)
{
	struct LIST_ListSC nlist;

	LIST_init(&nlist);

	/* when concatenating two empty list */
	LIST_concatenateUnshift(&listS, &nlist);

	/* both lists must remain empty */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));
	ck_assert_uint_eq(LIST_length(&listS), 0);
	ck_assert(LIST_empty(&listS));

	testSIntegrity(&listS);
	testSIntegrity(&nlist);
}
END_TEST

START_TEST(test_S_concatenateUnshift)
{
	struct LIST_ListSC nlist;

	LIST_init(&nlist);

	size_t i;
	struct E e[20];

	/* when pushing 10 links to two lists */
	for (i = 0; i < 10; ++i) {
		LIST_push(&listS, LIST_link(&e[i + 10], listS));
		LIST_push(&nlist, LIST_link(&e[i], listS));
	}

	/* and concatenating the lists */
	LIST_concatenateUnshift(&listS, &nlist);

	/* the source list must be empty */
	ck_assert_uint_eq(LIST_length(&nlist), 0);
	ck_assert(LIST_empty(&nlist));
	/* and the destination list must contain all links in order */
	ck_assert_uint_eq(LIST_length(&listS), 20);

	struct LIST_LinkS * link;
	for (i = 0, link = LIST_begin(&listS); link != LIST_end(&listS); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listS));
	ck_assert_uint_eq(i, 20);

	testSIntegrity(&listS);
	testSIntegrity(&nlist);
}
END_TEST

START_TEST(test_S_fromVector)
{
	struct E e[] = { {5}, {6}, {7} };
	/* build list from a vector */
	LIST_fromVector(&listS, e, listS, 3);

	/* list length should be 3 */
	ck_assert_uint_eq(LIST_length(&listS), 3);
	size_t i;
	/* and all links should be in the list in order */
	struct LIST_LinkS * link;
	for (i = 0, link = LIST_begin(&listS); link != LIST_end(&listS); ++i, link = LIST_next(link))
		ck_assert_ptr_eq(link, LIST_link(&e[i], listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_fromVectorEmpty)
{
	struct E e[] = { };
	/* build a list from an empty vector */
	LIST_fromVector(&listS, e, listS, 0);

	/* the list should also be empty */
	ck_assert(LIST_empty(&listS));
	ck_assert_uint_eq(LIST_length(&listS), 0);

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_foreach)
{
	struct E e[10];

	/* build a list with 10 links */
	LIST_fromVector(&listS, e, listS, 10);

	/* iterate over the list using a foreach-loop */
	struct LIST_LinkS * link;
	size_t i = 0;
	LIST_foreach(&listS, link)
		ck_assert_ptr_eq(link, LIST_link(&e[i++], listS));

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_foreach_break)
{
	size_t i;
	struct E e[10];

	/* build a list with 10 links */
	for (i = 0; i < 10; ++i) {
		e[i].n = i;
		LIST_push(&listS, LIST_link(&e[i], listS));
	}

	/* iterate over the list using a foreach-loop */
	struct LIST_LinkS * link;
	i = 0;
	LIST_foreach(&listS, link) {
		struct E * it = LIST_item(link, struct E, listS);
		if (it == &e[5]) /* break at the fifth link */
			break;
		/* change the first 5 items */
		it->n += 30;
	}

	/* check whether break has done its work */
	i = 30;
	LIST_foreach(&listS, link) {
		ck_assert_uint_eq(LIST_item(link, struct E, listS)->n, i);
		if (++i == 35)
			i = 5;
	}

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_foreach_item)
{
	struct E e[10];

	/* build a list with 10 links */
	LIST_fromVector(&listS, e, listS, 10);

	/* iterate over the items using a foreach-loop */
	struct E * it;
	size_t i = 0;
	LIST_foreachItem(&listS, it, listS)
		ck_assert_ptr_eq(it, &e[i++]);

	testSIntegrity(&listS);
}
END_TEST

START_TEST(test_S_foreach_item_cont)
{
	struct E e[10];

	/* build a list with 10 links */
	LIST_fromVector(&listS, e, listS, 10);

	/* get the _i-th elements */
	struct E * it = LIST_item(LIST_at(&listS, _i), struct E, listS);
	/* iterate over the items from there using a foreach-loop */
	size_t i = _i + 1;
	LIST_foreachItem_continue(&listS, it, listS)
		ck_assert_ptr_eq(it, &e[i++]);

	testSIntegrity(&listS);
}
END_TEST

static Suite * listD_suite()
{
	Suite * s = suite_create("ListD");

	TCase * tc = tcase_create("Ref");
	tcase_add_test(tc, test_D_item2link);
	tcase_add_test(tc, test_D_link2item);
	suite_add_tcase(s, tc);

	tc = tcase_create("InitStatic");
	tcase_add_test(tc, test_D_init_static);
	tcase_add_test(tc, test_DC_init_static);
	suite_add_tcase(s, tc);

	tc = tcase_create("Init");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_init);
	tcase_add_test(tc, test_D_empty);
	tcase_add_test(tc, test_D_count0);
	tcase_add_test(tc, test_D_begin0);
	tcase_add_test(tc, test_D_link2itemSafe);
	suite_add_tcase(s, tc);

	tc = tcase_create("OneList");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_push1);
	tcase_add_test(tc, test_D_begin1);
	tcase_add_test(tc, test_D_beginRev1);
	tcase_add_test(tc, test_D_next1);
	tcase_add_test(tc, test_D_prev1);
	suite_add_tcase(s, tc);

	tc = tcase_create("Push");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_push);
	suite_add_tcase(s, tc);

	tc = tcase_create("Iterate");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_iterate_rev);
	suite_add_tcase(s, tc);

	tc = tcase_create("Jump");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_loop_test(tc, test_D_forward, 0, 10);
	tcase_add_test(tc, test_D_forwardEnd);
	tcase_add_loop_test(tc, test_D_rewind, 0, 10);
	tcase_add_test(tc, test_D_rewindEnd);
	tcase_add_loop_test(tc, test_D_at, 0, 10);
	tcase_add_test(tc, test_D_atEnd);
	suite_add_tcase(s, tc);

	tc = tcase_create("Pop");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_pop0);
	tcase_add_test(tc, test_D_push_pop1);
	tcase_add_test(tc, test_D_push_pop);
	suite_add_tcase(s, tc);

	tc = tcase_create("Unshift");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_unshift1);
	tcase_add_test(tc, test_D_unshift);
	suite_add_tcase(s, tc);

	tc = tcase_create("Shift");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_shift0);
	tcase_add_test(tc, test_D_unshift_shift1);
	tcase_add_test(tc, test_D_unshift_shift);
	suite_add_tcase(s, tc);

	tc = tcase_create("Unlink");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_unlink0);
	tcase_add_loop_test(tc, test_D_unlink, 0, 10);
	suite_add_tcase(s, tc);

	tc = tcase_create("Insert");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_loop_test(tc, test_D_insert, 0, 10);
	suite_add_tcase(s, tc);

	tc = tcase_create("Queue");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_push_shift);
	tcase_add_test(tc, test_D_unshift_pop);
	suite_add_tcase(s, tc);

	tc = tcase_create("Seperate");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_loop_test(tc, test_D_separateFrom, 0, 10);
	tcase_add_test(tc, test_D_separateFromEnd);
	tcase_add_test(tc, test_D_separateFromEmpty);
	tcase_add_loop_test(tc, test_D_separateUntil, 0, 10);
	tcase_add_test(tc, test_D_separateUntilEnd);
	tcase_add_test(tc, test_D_separateUntilEmpty);
	suite_add_tcase(s, tc);

	tc = tcase_create("ConcatenatePush");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_concatenatePush_toEmpty);
	tcase_add_test(tc, test_D_concatenatePush_fromEmpty);
	tcase_add_test(tc, test_D_concatenatePush_twoEmpty);
	tcase_add_test(tc, test_D_concatenatePush);
	suite_add_tcase(s, tc);

	tc = tcase_create("ConcatenateUnshift");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_concatenateUnshift_toEmpty);
	tcase_add_test(tc, test_D_concatenateUnshift_fromEmpty);
	tcase_add_test(tc, test_D_concatenateUnshift_twoEmpty);
	tcase_add_test(tc, test_D_concatenateUnshift);
	suite_add_tcase(s, tc);

	tc = tcase_create("FromArray");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_fromVector);
	tcase_add_test(tc, test_D_fromVectorEmpty);
	suite_add_tcase(s, tc);

	tc = tcase_create("Foreach");
	tcase_add_checked_fixture(tc, &setupD, NULL);
	tcase_add_test(tc, test_D_foreach);
	tcase_add_test(tc, test_D_foreach_break);
	tcase_add_test(tc, test_D_foreach_unlink);
	tcase_add_test(tc, test_D_foreach_rev);
	tcase_add_test(tc, test_D_foreach_rev_break);
	tcase_add_test(tc, test_D_foreach_rev_unlink);
	tcase_add_test(tc, test_D_foreach_item);
	tcase_add_loop_test(tc, test_D_foreach_item_cont, 0, 10);
	tcase_add_test(tc, test_D_foreach_item_rev);
	tcase_add_loop_test(tc, test_D_foreach_item_rev_cont, 0, 10);
	suite_add_tcase(s, tc);

	return s;
}

static Suite * listS_suite()
{
	Suite * s = suite_create("ListS");

	TCase * tc = tcase_create("Ref");
	tcase_add_test(tc, test_S_item2link);
	tcase_add_test(tc, test_S_link2item);
	suite_add_tcase(s, tc);

	tc = tcase_create("InitStatic");
	tcase_add_test(tc, test_S_init_static);
	tcase_add_test(tc, test_SC_init_static);
	suite_add_tcase(s, tc);

	tc = tcase_create("Init");
	tcase_add_checked_fixture(tc, &setupS, NULL);
	tcase_add_test(tc, test_S_init);
	tcase_add_test(tc, test_S_empty);
	tcase_add_test(tc, test_S_count0);
	tcase_add_test(tc, test_S_begin0);
	tcase_add_test(tc, test_S_link2itemSafe);
	suite_add_tcase(s, tc);

	tc = tcase_create("OneList");
	tcase_add_checked_fixture(tc, &setupS, NULL);
	tcase_add_test(tc, test_S_push1);
	tcase_add_test(tc, test_S_begin1);
	tcase_add_test(tc, test_S_next1);
	suite_add_tcase(s, tc);

	tc = tcase_create("Push");
	tcase_add_checked_fixture(tc, &setupS, NULL);
	tcase_add_test(tc, test_S_push);
	suite_add_tcase(s, tc);

	tc = tcase_create("Jump");
	tcase_add_checked_fixture(tc, &setupS, NULL);
	tcase_add_loop_test(tc, test_S_forward, 0, 10);
	tcase_add_test(tc, test_S_forwardEnd);
	tcase_add_loop_test(tc, test_S_at, 0, 10);
	tcase_add_test(tc, test_S_atEnd);
	suite_add_tcase(s, tc);

	tc = tcase_create("Unshift");
	tcase_add_checked_fixture(tc, &setupS, NULL);
	tcase_add_test(tc, test_S_unshift1);
	tcase_add_loop_test(tc, test_S_unshift, 1, 10);
	suite_add_tcase(s, tc);

	tc = tcase_create("Shift");
	tcase_add_checked_fixture(tc, &setupS, NULL);
	tcase_add_test(tc, test_S_shift0);
	tcase_add_test(tc, test_S_unshift_shift1);
	tcase_add_test(tc, test_S_unshift_shift);
	suite_add_tcase(s, tc);

	tc = tcase_create("Queue");
	tcase_add_checked_fixture(tc, &setupS, NULL);
	tcase_add_test(tc, test_S_push_shift);
	suite_add_tcase(s, tc);

	tc = tcase_create("ConcatenatePush");
	tcase_add_checked_fixture(tc, &setupS, NULL);
	tcase_add_test(tc, test_S_concatenatePush_toEmpty);
	tcase_add_test(tc, test_S_concatenatePush_fromEmpty);
	tcase_add_test(tc, test_S_concatenatePush_twoEmpty);
	tcase_add_test(tc, test_S_concatenatePush);
	suite_add_tcase(s, tc);

	tc = tcase_create("ConcatenateUnshift");
	tcase_add_checked_fixture(tc, &setupS, NULL);
	tcase_add_test(tc, test_S_concatenateUnshift_toEmpty);
	tcase_add_test(tc, test_S_concatenateUnshift_fromEmpty);
	tcase_add_test(tc, test_S_concatenateUnshift_twoEmpty);
	tcase_add_test(tc, test_S_concatenateUnshift);
	suite_add_tcase(s, tc);

	tc = tcase_create("FromArray");
	tcase_add_checked_fixture(tc, &setupS, NULL);
	tcase_add_test(tc, test_S_fromVector);
	tcase_add_test(tc, test_S_fromVectorEmpty);
	suite_add_tcase(s, tc);

	tc = tcase_create("Foreach");
	tcase_add_checked_fixture(tc, &setupS, NULL);
	tcase_add_test(tc, test_S_foreach);
	tcase_add_test(tc, test_S_foreach_break);
	tcase_add_test(tc, test_S_foreach_item);
	tcase_add_loop_test(tc, test_S_foreach_item_cont, 0, 10);
	suite_add_tcase(s, tc);

	return s;
}

int main()
{
	SRunner * sr = srunner_create(listD_suite());
	srunner_add_suite(sr, listS_suite());
	srunner_set_tap(sr, "-");
	srunner_run_all(sr, CK_NORMAL);
	srunner_free(sr);
	return EXIT_SUCCESS;
}
