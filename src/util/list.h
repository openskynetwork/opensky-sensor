/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_LIST_H
#define _HAVE_LIST_H

#include <stdlib.h>
#include <stdbool.h>
#include "util.h"

#ifdef head
#error macro "head" is already defined, maybe it helps to include this file \
	earlier
#endif

#ifdef tail
#error macro "tail" is already defined, maybe it helps to include this file \
	earlier
#endif

/** Representation of link of a double linked list. These links will be linked,
 *   forming the list. The actual items will contain the links and will be
 *   accessed using some pointer arithmetic.
 */
struct LIST_LinkD {
	/** Pointer to next link */
	struct LIST_LinkD * next;
	/** Pointer to previous link */
	struct LIST_LinkD * prev;
};

/** A double linked list. */
struct LIST_ListD {
	/** Anchor link. The list is circular linked with this link as dummy */
	struct LIST_LinkD anchor;
#define head anchor.next
#define tail anchor.prev
};

/** A double linked, counted list. */
struct LIST_ListDC {
	/** Actual list */
	struct LIST_ListD listD;
	/** Counter */
	size_t length;
	/** Lazy counting: length is only valid if tainted is false */
	bool tainted;
};

/** Initialize the list. Must be called before using the list,
 *   except for replacing operations, such as LIST_separateFromD().
 * @note Can also be used to clear the list. When clearing the list, be sure to
 *   have access to the list in another way.
 * @param list list to be initialized.
 */
static inline void LIST_initD(struct LIST_ListD * list)
{
	/* anchor: points to itself in both directions */
	list->head = list->tail = &list->anchor;
}

/** Test whether the list is empty.
 * @param list list to be tested.
 * @return true iff list is empty.
 */
static inline bool LIST_emptyD(const struct LIST_ListD * list)
{
	/* list is empty if the anchor points to itself */
	return list->head == &list->anchor;
}

/** Unlink a link from the list.
 * @param list list to be manipulated.
 * @param link to be unlinked. Must be part of the list. If the link is
 *  the lists anchor, nothing will happen.
 * @return the link itself.
 * @note the list itself is not really needed here, but is here to unify the
 *  interface.
 */
static inline struct LIST_LinkD * LIST_unlinkD(struct LIST_ListD * list,
	struct LIST_LinkD * link)
{
	/* unlink by redirecting pointers of the previous and next link */
	link->prev->next = link->next;
	link->next->prev = link->prev;
	return link;
}

/** Unlink first link of the list and return it.
 * @note if the list is empty, the list will still be empty but the anchor
 *  is returned. Be sure to check this condition.
 * @param list list to be shifted.
 * @return first link of the list or the anchor if the list was empty.
 */
static inline struct LIST_LinkD * LIST_shiftD(struct LIST_ListD * list)
{
	/* shift = unlink head */
	return LIST_unlinkD(list, list->head);
}

/** Unlink last link of the list and return it.
 * @note if the list is empty, the list will still be empty but the anchor
 *  is returned. Be sure to check this condition.
 * @param list list to be popped.
 * @return last link of the list or the anchor if the list was empty.
 */
static inline struct LIST_LinkD * LIST_popD(struct LIST_ListD * list)
{
	/* pop = unlink tail */
	return LIST_unlinkD(list, list->tail);
}

/** Insert a new link after another link which is member of the list.
 * @param list list to be manipulated.
 * @param prev link after which the new link will be inserted. This link must
 *  be part of a list.
 * @param link link to be inserted after prev. Must not be part of another list.
 * @note the list itself is not really needed here, but is here to unify the
 *  interface.
 */
static inline void LIST_insertD(struct LIST_ListD * list,
	struct LIST_LinkD * __restrict prev,
	struct LIST_LinkD * __restrict link)
{
	/* link by redirecting pointers of the previous and next link */
	link->next = prev->next;
	link->prev = prev;
	prev->next->prev = link;
	prev->next = link;
}

/** Insert a new link at the end of the list.
 * @param list list to be manipulated.
 * @param link link to be inserted at the end. Must not be part of another list.
 */
static inline void LIST_pushD(struct LIST_ListD * list,
	struct LIST_LinkD * link)
{
	/* push = insert after tail */
	LIST_insertD(list, list->tail, link);
}

/** Insert a new link at the beginning of the list.
 * @param list list to be manipulated.
 * @param link link to be inserted at the beginning. Must not be part of another
 *  list.
 */
static inline void LIST_unshiftD(struct LIST_ListD * list,
	struct LIST_LinkD * link)
{
	/* unshift = insert before head = insert after anchor */
	LIST_insertD(list, &list->anchor, link);
}

/** Separate the lists tail beginning from (including) a pivot into a new list.
 * @param list list to be separated. Will be cut (excluding) at the pivot.
 * @param newList list to hold the tail beginning (including) at the pivot. May
 *  be uninitialized.
 * @param pivot pivot link where the list is separated. Must be part of
 *  the source list.
 * @note if the pivot is the anchor, the source list will be left untouched and
 *  the new list will be cleared.
 * @note if the pivot is the head, the source list will be emptied and the
 *  destination list will hold all links.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_separateFromD(struct LIST_ListD * __restrict list,
	struct LIST_ListD * __restrict newList, struct LIST_LinkD * pivot)
{
	if (likely(pivot != &list->anchor)) {
		/* note: we also know, that the source list is not empty here,
		 *  otherwise the pivot would have been the anchor */

		/* destination list: head is pivot, tail is old lists tail */
		newList->head = pivot;
		newList->tail = list->tail;
		newList->tail->next = &newList->anchor;

		/* source list: head is left untouched, tail is the predecessor of the
		 *  pivot */
		list->tail = pivot->prev;
		list->tail->next = &list->anchor;
		pivot->prev = &newList->anchor;
	} else {
		/* Also the case if the source list was empty. Prevent linking the
		 * anchor of the source list to the destination list. */
		LIST_initD(newList);
	}
}

/** Separate the lists head beginning from (excluding) a pivot into a new list.
 * @param list list to be separated. Will be cut (including) at the pivot and
 *  include the tail only.
 * @param newList list to hold the head until (excluding) the pivot. May be
 *  uninitialized.
 * @param pivot pivot link where the list is separated. Must be part of
 *  the source list.
 * @note if the pivot is the anchor, the source list will be emptied and
 *  the destination list will hold all links.
 * @note if the pivot is the source lists head, the source list will be left
 *  untouched and the destination list is emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_separateUntilD(struct LIST_ListD * __restrict list,
	struct LIST_ListD * __restrict newList, struct LIST_LinkD * pivot)
{
	if (likely(pivot != list->head)) {
		/* note: we also know, that the source list is not empty here,
		 *  otherwise the pivot would have been the head */

		/* destination list: head is source head, tail is the predecessor of the
		 *  pivot */
		newList->head = list->head;
		newList->head->prev = &newList->anchor;
		newList->tail = pivot->prev;
		newList->tail->next = &newList->anchor;

		/* source list: tail is left untouched, tail is the pivot */
		list->head = pivot;
		pivot->prev = &list->anchor;
	} else {
		/* Also the case if the source list was empty. Prevent linking the
		 *  anchor of the source list to the destination list. */
		LIST_initD(newList);
	}
}

/** Concatenate two lists by appending a source list to a destination list.
 * @param dst destination list. Will contain its old links followed by the
 *  links of the source list.
 * @param src source list. Will be emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_concatenatePushD(struct LIST_ListD * __restrict dst,
	struct LIST_ListD * __restrict src)
{
	/* If the source list is empty, do nothing. Especially: do not link
	 *  its anchor into the destination. */
	if (!LIST_emptyD(src)) {
		src->head->prev = dst->tail;
		src->tail->next = &dst->anchor;
		dst->tail->next = src->head;
		dst->tail = src->tail;

		LIST_initD(src);
	}
}

/** Concatenate two lists by prepending a source list to a destination list.
 * @param dst destination list. Will contain the links of the source list
 *  followed by its initial links.
 * @param src source list. Will be emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_concatenateUnshiftD(struct LIST_ListD * __restrict dst,
	struct LIST_ListD * __restrict src)
{
	/* If the source list is empty, do nothing. Especially: do not link
	 *  its anchor into the destination. */
	if (!LIST_emptyD(src)) {
		src->head->prev = &dst->anchor;
		src->tail->next = dst->head;
		dst->head->prev = src->tail;
		dst->head = src->head;

		LIST_initD(src);
	}
}

/** Create list from a vector.
 * @param dst destination list. May be uninitialized.
 * @param vector pointer to elements of a vector.
 * @param offset offset of the link inside a vectors' element.
 * @param items number of items of the vector.
 * @param elemSize size of one element in the vector.
 */
static inline void LIST_fromVectorD(struct LIST_ListD * dst, void * vector,
	ptrdiff_t offset, size_t items, size_t elemSize)
{
	/* get link of first element */
	struct LIST_LinkD * link = (struct LIST_LinkD*)((char*)vector + offset);
	/* prev will always point to the predecessor: begin at the anchor */
	struct LIST_LinkD * prev = &dst->anchor;

	while (items--) {
		/* establish links */
		link->prev = prev;
		prev->next = link;
		/* iterate: next element, next link */
		prev = link;
		vector = (char*)vector + elemSize;
		link = (struct LIST_LinkD*)((char*)vector + offset);
	}
	/* finalize list */
	prev->next = &dst->anchor;
	dst->tail = prev;
}

/** Get first link of the list.
 * @param list the list.
 * @return first link of the list.
 */
static inline struct LIST_LinkD * LIST_beginD(struct LIST_ListD * list)
{
	return list->head;
}

/** Get last link of the list.
 * @param list the list.
 * @return last link of the list.
 */
static inline struct LIST_LinkD * LIST_beginRevD(struct LIST_ListD * list)
{
	return list->tail;
}

/** Get anchor of the list.
 * @param list the list.
 * @return anchor of the list.
 */
static inline struct LIST_LinkD * LIST_endD(struct LIST_ListD * list)
{
	return &list->anchor;
}

/** Get n-th link of the list.
 * @param list the list.
 * @param n number of links to iterate forward.
 * @return n-th link of the list.
 */
static inline struct LIST_LinkD * LIST_atD(struct LIST_ListD * list, size_t n)
{
	struct LIST_LinkD * link = list->head;
	while (n--) link = link->next;
	return link;
}


/** Initialize the list. Must be called before using the list,
 *   except for replacing operations, such as LIST_separateDC().
 * @note Can also be used to clear the list. When clearing the list, be sure to
 *   have access to the list in another way.
 * @param list list to be initialized.
 */
static inline void LIST_initDC(struct LIST_ListDC * list)
{
	LIST_initD(&list->listD);
	list->length = 0;
	list->tainted = false;
}

/** Test whether the list is empty.
 * @param list list to be tested.
 * @return true iff list is empty.
 */
static inline bool LIST_emptyDC(const struct LIST_ListDC * list)
{
	return LIST_emptyD(&list->listD);
}

/** Get length of the list.
 * @param list the list.
 * @return number of links in the list.
 * @note if the list is tainted, the links will be counted in O(n)
 */
static inline size_t LIST_lengthDC(const struct LIST_ListDC * list)
{
	if (unlikely(list->tainted)) {
		/* list is tainted: count links and safe for next access */
		struct LIST_ListDC * list_uc = (struct LIST_ListDC*)list;
		struct LIST_LinkD * link;
		list_uc->length = 0;
		for (link = list_uc->listD.head; link != &list_uc->listD.anchor; link = link->next)
			++list_uc->length;
		list_uc->tainted = false;
	}
	return list->length;
}

/** Unlink a link from the list.
 * @param list list to be manipulated.
 * @param link to be unlinked. Must be part of the list. If the link is
 *  the lists anchor, nothing will happen.
 * @return the link itself.
 */
static inline struct LIST_LinkD * LIST_unlinkDC(struct LIST_ListDC * list,
	struct LIST_LinkD * link)
{
	if (likely(list->tainted || list->length)) {
		LIST_unlinkD(&list->listD, link);
		--list->length;
	}
	return link;
}

/** Unlink first link of the list and return it.
 * @note if the list is empty, the list will still be empty but the anchor
 *  is returned. Be sure to check this condition.
 * @param list list to be shifted.
 * @return first link of the list or the anchor if the list was empty.
 */
static inline struct LIST_LinkD * LIST_shiftDC(struct LIST_ListDC * list)
{
	/* shift = unlink head */
	return LIST_unlinkDC(list, list->listD.head);
}

/** Unlink last link of the list and return it.
 * @note if the list is empty, the list will still be empty but the anchor
 *  is returned. Be sure to check this condition.
 * @param list list to be popped.
 * @return last link of the list or the anchor if the list was empty.
 */
static inline struct LIST_LinkD * LIST_popDC(struct LIST_ListDC * list)
{
	/* pop = unlink tail */
	return LIST_unlinkDC(list, list->listD.tail);
}

/** Insert a new link after another link which is member of the list.
 * @param prev link after which the new link will be inserted. This link must
 *  be part of a list.
 * @param link link to be inserted after prev. Must not be part of another list.
 */
static inline void LIST_insertDC(struct LIST_ListDC * list,
	struct LIST_LinkD * __restrict prev, struct LIST_LinkD * __restrict link)
{
	LIST_insertD(&list->listD, prev, link);
	++list->length;
}

/** Insert a new link at the end of the list.
 * @param list list to be manipulated.
 * @param link link to be inserted at the end. Must not be part of another list.
 */
static inline void LIST_pushDC(struct LIST_ListDC * list,
	struct LIST_LinkD * link)
{
	LIST_insertDC(list, list->listD.tail, link);
}

/** Insert a new link at the beginning of the list.
 * @param list list to be manipulated.
 * @param link link to be inserted at the beginning. Must not be part of another
 *  list.
 */
static inline void LIST_unshiftDC(struct LIST_ListDC * list,
	struct LIST_LinkD * link)
{
	LIST_insertDC(list, &list->listD.anchor, link);
}

/** Separate the lists tail beginning from (including) a pivot into a new list.
 * @param list list to be separated. Will be cut (excluding) at the pivot.
 * @param newList list to hold the tail beginning (including) at the pivot. May
 *  be uninitialized.
 * @param pivot pivot link where the list is separated. Must be part of
 *  the source list.
 * @note if the pivot is the anchor, the source list will be left untouched and
 *  the new list will be cleared.
 * @note if the pivot is the head, the source list will be emptied and the
 *  destination list will hold all links.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_separateFromDC(struct LIST_ListDC * __restrict list,
	struct LIST_ListDC * __restrict newList, struct LIST_LinkD * pivot)
{
	if (pivot != &list->listD.anchor) {
		LIST_separateFromD(&list->listD, &newList->listD, pivot);
		list->tainted = newList->tainted = true;
	} else {
		LIST_initDC(newList);
	}
}

/** Separate the lists tail beginning from (including) a pivot into a new list
 *  and fix the counting (eager counting).
 * @param list list to be separated. Will be cut (excluding) at the pivot.
 * @param newList list to hold the tail beginning (including) at the pivot. May
 *  be uninitialized.
 * @param pivot pivot link where the list is separated. Must be part of
 *  the source list.
 * @note if the pivot is the anchor, the source list will be left untouched and
 *  the new list will be cleared.
 * @note if the pivot is the head, the source list will be emptied and the
 *  destination list will hold all links.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_separateFromFixDC(struct LIST_ListDC * __restrict list,
	struct LIST_ListDC * __restrict newList, struct LIST_LinkD * pivot)
{
	LIST_separateFromDC(list, newList, pivot);
	LIST_lengthDC(newList);
	list->length -= newList->length;
	list->tainted = false;
}

/** Separate the lists head beginning from (excluding) a pivot into a new list.
 * @param list list to be separated. Will be cut (including) at the pivot and
 *  include the tail only.
 * @param newList list to hold the head until (excluding) the pivot. May be
 *  uninitialized.
 * @param pivot pivot link where the list is separated. Must be part of
 *  the source list.
 * @note if the pivot is the anchor, the source list will be emptied and
 *  the destination list will hold all links.
 * @note if the pivot is the source lists head, the source list will be left
 *  untouched and the destination list is emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_separateUntilDC(struct LIST_ListDC * __restrict list,
	struct LIST_ListDC * __restrict newList, struct LIST_LinkD * pivot)
{
	LIST_separateUntilD(&list->listD, &newList->listD, pivot);
	list->tainted = newList->tainted = true;
}

/** Separate the lists head beginning from (excluding) a pivot into a new list
 *  and fix the counting (eager counting).
 * @param list list to be separated. Will be cut (including) at the pivot and
 *  include the tail only.
 * @param newList list to hold the head until (excluding) the pivot. May be
 *  uninitialized.
 * @param pivot pivot link where the list is separated. Must be part of
 *  the source list.
 * @note if the pivot is the anchor, the source list will be emptied and
 *  the destination list will hold all links.
 * @note if the pivot is the source lists head, the source list will be left
 *  untouched and the destination list is emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_separateUntilFixDC(struct LIST_ListDC * __restrict list,
	struct LIST_ListDC * __restrict newList, struct LIST_LinkD * pivot)
{
	LIST_separateUntilDC(list, newList, pivot);
	LIST_lengthDC(newList);
	list->length -= newList->length;
	list->tainted = false;
}

/** Concatenate two lists by appending a source list to a destination list.
 * @param dst destination list. Will contain its old links followed by the
 *  links of the source list.
 * @param src source list. Will be emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_concatenatePushDC(struct LIST_ListDC * __restrict dst,
	struct LIST_ListDC * __restrict src)
{
	if (likely(!LIST_emptyDC(src))) {
		LIST_concatenatePushD(&dst->listD, &src->listD);
		dst->tainted = dst->tainted || src->tainted;
		dst->length += src->length;

		LIST_initDC(src);
	}
}

/** Concatenate two lists by prepending a source list to a destination list.
 * @param dst destination list. Will contain the links of the source list
 *  followed by its initial links.
 * @param src source list. Will be emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_concatenateUnshiftDC(struct LIST_ListDC * __restrict dst,
	struct LIST_ListDC * __restrict src)
{
	if (likely(!LIST_emptyDC(src))) {
		LIST_concatenateUnshiftD(&dst->listD, &src->listD);
		dst->tainted = dst->tainted || src->tainted;
		dst->length += src->length;

		LIST_initDC(src);
	}
}

/** Create list from a vector.
 * @param dst destination list. May be uninitialized.
 * @param vector pointer to elements of a vector.
 * @param offset offset of the link inside a vectors' element.
 * @param items number of items of the vector.
 * @param elemSize size of one element in the vector.
 */
static inline void LIST_fromVectorDC(struct LIST_ListDC * dst, void * vector,
	ptrdiff_t offset, size_t items, size_t elemSize)
{
	LIST_fromVectorD(&dst->listD, vector, offset, items, elemSize);
	dst->length = items;
	dst->tainted = false;
}

/** Get first link of the list.
 * @param list the list.
 * @return first link of the list.
 */
static inline struct LIST_LinkD * LIST_beginDC(struct LIST_ListDC * list)
{
	return LIST_beginD(&list->listD);
}

/** Get last link of the list.
 * @param list the list.
 * @return last link of the list.
 */
static inline struct LIST_LinkD * LIST_beginRevDC(struct LIST_ListDC * list)
{
	return LIST_beginRevD(&list->listD);
}

/** Get anchor of the list.
 * @param list the list.
 * @return anchor of the list.
 */
static inline struct LIST_LinkD * LIST_endDC(struct LIST_ListDC * list)
{
	return LIST_endD(&list->listD);
}

/** Get n-th link of the list.
 * @param list the list.
 * @param n number of links to iterate forward.
 * @return n-th link of the list.
 */
static inline struct LIST_LinkD * LIST_atDC(struct LIST_ListDC * list, size_t n)
{
	return LIST_atD(&list->listD, n);
}

#undef head
#undef tail

/** Representation of link of a single linked list. These links will be linked,
 *   forming the list. The actual items will contain the links and will be
 *   accessed using some pointer arithmetic.
 */
struct LIST_LinkS {
	/** Pointer to next link */
	struct LIST_LinkS * next;
};

/** A single linked list. */
struct LIST_ListS {
	/** Anchor link. The list is circular linked with this link as dummy */
	struct LIST_LinkS anchor;
#define head anchor.next
	/** Pointer to the lists' tail */
	struct LIST_LinkS * tail;
};

/** A single linked, counted list. */
struct LIST_ListSC {
	/** Actual list */
	struct LIST_ListS listS;
	/** Counter */
	size_t length;
};

/** Initialize the list. Must be called before using the list.
 * @note Can also be used to clear the list. When clearing the list, be sure to
 *   have access to the list in another way.
 * @param list list to be initialized.
 */
static inline void LIST_initS(struct LIST_ListS * list)
{
	/* anchor: points to itself, tail points to anchor */
	list->head = list->tail = &list->anchor;
}

/** Test whether the list is empty.
 * @param list list to be tested.
 * @return true iff list is empty.
 */
static inline bool LIST_emptyS(const struct LIST_ListS * list)
{
	/* list is empty, if the anchor points to itself */
	return list->head == &list->anchor;
}

/** Unlink first link of the list and return it.
 * @note if the list is empty, the list will still be empty but the anchor
 *  is returned. Be sure to check this condition.
 * @param list list to be shifted.
 * @return first link of the list or the anchor if the list was empty.
 */
static inline struct LIST_LinkS * LIST_shiftS(struct LIST_ListS * list)
{
	struct LIST_LinkS * link = list->head;
	list->head = link->next;
	if (unlikely(LIST_emptyS(list)))
		list->tail = &list->anchor;
	return link;
}

/** Insert a new link at the end of the list.
 * @param list list to be manipulated.
 * @param link link to be inserted at the end. Must not be part of another list.
 */
static inline void LIST_pushS(struct LIST_ListS * list,
	struct LIST_LinkS * link)
{
	link->next = &list->anchor;
	list->tail->next = link;
	list->tail = link;
}

/** Insert a new link at the beginning of the list.
 * @param list list to be manipulated.
 * @param link link to be inserted at the beginning. Must not be part of another
 *  list.
 */
static inline void LIST_unshiftS(struct LIST_ListS * list,
	struct LIST_LinkS * link)
{
	link->next = list->head;
	if (unlikely(LIST_emptyS(list)))
		list->tail = link;
	list->head = link;
}

/** Concatenate two lists by appending a source list to a destination list.
 * @param dst destination list. Will contain its old links followed by the
 *  links of the source list.
 * @param src source list. Will be emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_concatenatePushS(struct LIST_ListS * __restrict dst,
	struct LIST_ListS * __restrict src)
{
	if (likely(!LIST_emptyS(src))) {
		src->tail->next = &dst->anchor;
		dst->tail->next = src->head;
		dst->tail = src->tail;

		LIST_initS(src);
	}
}

/** Concatenate two lists by prepending a source list to a destination list.
 * @param dst destination list. Will contain the links of the source list
 *  followed by its initial links.
 * @param src source list. Will be emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_concatenateUnshiftS(struct LIST_ListS * __restrict dst,
	struct LIST_ListS * __restrict src)
{
	if (likely(!LIST_emptyS(src))) {
		src->tail->next = dst->head;
		if (LIST_emptyS(dst))
			dst->tail = src->head;
		dst->head = src->head;

		LIST_initS(src);
	}
}

/** Create list from a vector.
 * @param dst destination list. May be uninitialized.
 * @param vector pointer to elements of a vector.
 * @param offset offset of the link inside a vectors' element.
 * @param items number of items of the vector.
 * @param elemSize size of one element in the vector.
 */
static inline void LIST_fromVectorS(struct LIST_ListS * dst, void * vector,
	ptrdiff_t offset, size_t items, size_t elemSize)
{
	/* get link of first element */
	struct LIST_LinkS * link = (struct LIST_LinkS*)((char*)vector + offset);
	/* prev will always point to the predecessor: begin at the anchor */
	struct LIST_LinkS * prev = &dst->anchor;

	while (items--) {
		/* establish link */
		prev->next = link;
		/* iterate: next element, next link */
		prev = link;
		vector = (char*)vector + elemSize;
		link = (struct LIST_LinkS*)((char*)vector + offset);
	}
	/* finalize list */
	prev->next = &dst->anchor;
	dst->tail = prev;
}

/** Get first link of the list.
 * @param list the list.
 * @return first link of the list.
 */
static inline struct LIST_LinkS * LIST_beginS(struct LIST_ListS * list)
{
	return list->head;
}

/** Get anchor of the list.
 * @param list the list.
 * @return anchor of the list.
 */
static inline struct LIST_LinkS * LIST_endS(struct LIST_ListS * list)
{
	return &list->anchor;
}

/** Get n-th link of the list.
 * @param list the list.
 * @param n number of links to iterate forward.
 * @return n-th link of the list.
 */
static inline struct LIST_LinkS * LIST_atS(struct LIST_ListS * list, size_t n)
{
	struct LIST_LinkS * link = list->head;
	while (n--) link = link->next;
	return link;
}


/** Initialize the list. Must be called before using the list.
 * @note Can also be used to clear the list. When clearing the list, be sure to
 *   have access to the list in another way.
 * @param list list to be initialized.
 */
static inline void LIST_initSC(struct LIST_ListSC * list)
{
	LIST_initS(&list->listS);
	list->length = 0;
}

/** Test whether the list is empty.
 * @param list list to be tested.
 * @return true iff list is empty.
 */
static inline bool LIST_emptySC(const struct LIST_ListSC * list)
{
	return LIST_emptyS(&list->listS);
}

/** Get length of the list.
 * @param list the list.
 * @return number of links in the list.
 */
static inline size_t LIST_lengthSC(const struct LIST_ListSC * list)
{
	return list->length;
}

/** Unlink first link of the list and return it.
 * @note if the list is empty, the list will still be empty but the anchor
 *  is returned. Be sure to check this condition.
 * @param list list to be shifted.
 * @return first link of the list or the anchor if the list was empty.
 */
static inline struct LIST_LinkS * LIST_shiftSC(struct LIST_ListSC * list)
{
	if (likely(!LIST_emptySC(list))) {
		struct LIST_LinkS * ret = LIST_shiftS(&list->listS);
		--list->length;
		return ret;
	} else
		return list->listS.head;
}

/** Insert a new link at the end of the list.
 * @param list list to be manipulated.
 * @param link link to be inserted at the end. Must not be part of another list.
 */
static inline void LIST_pushSC(struct LIST_ListSC * list,
	struct LIST_LinkS * link)
{
	LIST_pushS(&list->listS, link);
	++list->length;
}

/** Insert a new link at the beginning of the list.
 * @param list list to be manipulated.
 * @param link link to be inserted at the beginning. Must not be part of another
 *  list.
 */
static inline void LIST_unshiftSC(struct LIST_ListSC * list,
	struct LIST_LinkS * link)
{
	LIST_unshiftS(&list->listS, link);
	++list->length;
}

/** Concatenate two lists by appending a source list to a destination list.
 * @param dst destination list. Will contain its old links followed by the
 *  links of the source list.
 * @param src source list. Will be emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_concatenatePushSC(struct LIST_ListSC * __restrict dst,
	struct LIST_ListSC * __restrict src)
{
	if (likely(!LIST_emptySC(src))) {
		LIST_concatenatePushS(&dst->listS, &src->listS);
		dst->length += src->length;

		LIST_initSC(src);
	}
}

/** Concatenate two lists by prepending a source list to a destination list.
 * @param dst destination list. Will contain the links of the source list
 *  followed by its initial links.
 * @param src source list. Will be emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
static inline void LIST_concatenateUnshiftSC(struct LIST_ListSC * __restrict dst,
	struct LIST_ListSC * __restrict src)
{
	if (likely(!LIST_emptySC(src))) {
		LIST_concatenateUnshiftS(&dst->listS, &src->listS);
		dst->length += src->length;

		LIST_initSC(src);
	}
}

/** Create list from a vector.
 * @param dst destination list. May be uninitialized.
 * @param vector pointer to elements of a vector.
 * @param offset offset of the link inside a vectors' element.
 * @param items number of items of the vector.
 * @param elemSize size of one element in the vector.
 */
static inline void LIST_fromVectorSC(struct LIST_ListSC * dst, void * vector,
	ptrdiff_t offset, size_t items, size_t elemSize)
{
	LIST_fromVectorS(&dst->listS, vector, offset, items, elemSize);
	dst->length = items;
}

/** Get first link of the list.
 * @param list the list.
 * @return first link of the list.
 */
static inline struct LIST_LinkS * LIST_beginSC(struct LIST_ListSC * list)
{
	return LIST_beginS(&list->listS);
}

/** Get anchor of the list.
 * @param list the list.
 * @return anchor of the list.
 */
static inline struct LIST_LinkS * LIST_endSC(struct LIST_ListSC * list)
{
	return LIST_endS(&list->listS);
}

/** Get n-th link of the list.
 * @param list the list.
 * @param n number of links to iterate forward.
 * @return n-th link of the list.
 */
static inline struct LIST_LinkS * LIST_atSC(struct LIST_ListSC * list, size_t n)
{
	return LIST_atS(&list->listS, n);
}

#undef head

/** Get a list item from its link.
 * @param link link
 * @param type item type
 * @param member name of member inside the type for accessing the link
 * @return item. Will return an invalid pointer if link is the anchor.
 */
#define LIST_item(link, type, member) \
	container_of(link, type, member)

/** Get a list item from its link.
 * @param link link
 * @param type item type
 * @param member name of member inside the type for accessing the link
 * @return item or NULL if link is the anchor.
 */
#define LIST_itemSafe(list, link, type, member) ({ \
	__typeof__(link) _link = (link); \
	unlikely(_link == LIST_end(list)) ? NULL : \
		container_of(_link, type, member); })

/** Get a link from the list item
 * @param item item
 * @return link
 */
#define LIST_link(item, member) &((item)->member)

/** Initialize (statically) a double linked counted list.
 * @param name list name
 * @return initializer expression for the list
 */
#define LIST_ListDC_INIT(name) \
	{ { { &name.listD.anchor, &name.listD.anchor } }, 0, false }

#if !defined(ECLIPSE) && defined(HAS_GENERIC)
/** Initialize (statically) a double linked list.
 * @param name list name
 * @return initializer expression for the list
 */
#define LIST_ListD_INIT(name) { { &name.anchor, &name.anchor } }

/** Initialize (statically) a single linked list.
 * @param name list name
 * @return initializer expression for the list
 */
#define LIST_ListS_INIT(name) { { &name.anchor }, &name.anchor }

/** Initialize (statically) a single linked counted list.
 * @param name list name
 * @return initializer expression for the list
 */
#define LIST_ListSC_INIT(name) \
	{ { { &name.listS.anchor }, &name.listS.anchor }, 0 }

/** Simple macro to concatenate 3 tokens in the preprocessor
 * @param t1 first token
 * @param t2 second token
 * @param t3 third token
 * @return the concatenation of all tokens without any spaces
 */
#define concat(t1, t2, t3) t1 ## t2 ## t3

/** Generator for calling a function depending on the list type. Only defined on
 *   counted lists.
 * @param dsc selector (normally: the list)
 * @param what function name (with out LIST_ prefix). The suffix is appended
 *  depending on the selectors' type.
 * @return assembled function name
 */
#define LIST_SelectC(dsc, what) \
	(_Generic(dsc, struct LIST_ListSC *: concat(LIST_, what, SC), \
		struct LIST_ListDC *: concat(LIST_, what, DC)))

/** Generator for calling a function depending on the list type. Only defined on
 *   double linked lists.
 * @param dsc selector (normally: the list)
 * @param what function name (with out LIST_ prefix). The suffix is appended
 *  depending on the selectors' type.
 * @return assembled function name
 */
#define LIST_SelectD(dsc, what) \
	(_Generic(dsc, struct LIST_ListD *: concat(LIST_, what, D), \
		struct LIST_ListDC *: concat(LIST_, what, DC)))

/** Generator for calling a function depending on the list type.
 * @param dsc selector (normally: the list)
 * @param what function name (with out LIST_ prefix). The suffix is appended
 *  depending on the selectors' type.
 * @return assembled function name
 */
#define LIST_SelectSD(dsc, what) \
	(_Generic(dsc, struct LIST_ListS *: concat(LIST_, what, S), \
		struct LIST_ListSC *: concat(LIST_, what, SC), \
		struct LIST_ListD *: concat(LIST_, what, D), \
		struct LIST_ListDC *: concat(LIST_, what, DC)))

/** Initialize the list. Must be called before using the list,
 *   except for replacing operations, such as LIST_separateFrom().
 * @note Can also be used to clear the list. When clearing the list, be sure to
 *   have acess to the list in another way.
 * @param list list to be initialized.
 */
#define LIST_init(list) LIST_SelectSD((list), init)(list)

/** Test whether the list is empty.
 * @param list list to be tested.
 * @return true iff list is empty.
 */
#define LIST_empty(list) LIST_SelectSD((list), empty)(list)

/** Get length of the list. Only defined on counted lists.
 * @param list the list.
 * @return number of links in the list.
 * @note if the list is a double linked list and is tainted, the links will be
 *  counted in O(n)
 */
#define LIST_length(list) LIST_SelectC((list), length)(list)

/** Get first link of the list.
 * @param list the list.
 * @return first link of the list.
 */
#define LIST_begin(list) LIST_SelectSD((list), begin)(list)

/** Get last link of the list. Only defined on double linked lists.
 * @param list the list.
 * @return first link of the list.
 */
#define LIST_beginRev(list) LIST_SelectD((list), beginRev)(list)

/** Get anchor of the list.
 * @param list the list.
 * @return anchor of the list.
 */
#define LIST_end(list) LIST_SelectSD((list), end)(list)

/** Get successor of a link.
 * @param link the link.
 * @return successor of the link.
 * @note if the link is the last link, the anchor will be returned.
 * @note if the link is the anchor, it will return the first link again.
 */
#define LIST_next(link) ((link)->next)

/** Get predecessor of a link. Only defined on links of double linked lists.
 * @param link the link.
 * @return successor of the link.
 * @note if the link is the first link, the anchor will be returned.
 * @note if the link is the anchor, it will return the last link again.
 */
#define LIST_prev(link) ((link)->prev)

/** Get n-th successor of a link.
 * @param link the link.
 * @return n-th successor of the link.
 * @note although the lists are circular, be sure to understand that the anchor
 *  is in between the last and the first link. Thus, when iterating after the
 *  end of the list, first the anchor will be taken and then the first link
 *  again. Best practice is to not iterate after the end of the list.
 */
#define LIST_forward(link, n) ({ \
	__typeof__(link) _link = (link); \
	__typeof__(n) _n = (n); \
	while (_n--) _link = LIST_next(_link); \
	_link; })

/** Get n-th predecessor of a link. Only defined on links of double linked
 *   lists.
 * @param link the link.
 * @return n-th predecessor of the link.
 * @note although the lists are circular, be sure to understand that the anchor
 *  is in between the last and the first link. Thus, when iterating before the
 *  begin of the list, first the anchor will be taken and then the last link
 *  again. Best practice is to not iterate before the beginning of the list.
 */
#define LIST_rewind(link, n) ({ \
	__typeof__(link) _link = (link); \
	__typeof__(n) _n = (n); \
	while (_n--) _link = LIST_prev(_link); \
	_link; })

/** Get n-th link of the list.
 * @param list the list.
 * @param n number of links to iterate forward.
 * @return n-th link of the list.
 * @note although the lists are circular, be sure to understand that the anchor
 *  is in between the last and the first link. Thus, when iterating after the
 *  end of the list, first the anchor will be taken and then the first link
 *  again. Best practice is that n is <= the length of the list.
 */
#define LIST_at(list, pos) \
	LIST_SelectSD((list), at)((list), (pos))

/** Insert a new link after another link which is member of the list. Only
 *   defined on double linked lists.
 * @param at link before which the new link will be inserted. This link must
 *  be part of a list.
 * @param link link to be inserted after prev. Must not be part of another list.
 * @note this has another semantics than the LIST_insert* functions: they are
 *  given the predecessor of the new link, i.e. the new link will be inserted
 *  after the given link. This macro will insert the new link BEFORE the given
 *  one. If 'at' is the n-th link of the list, it will be the n+1-th link and
 *  'link' will be the new n-th link.
 */
#define LIST_insert(list, at, link) \
	LIST_SelectD((list), insert)((list), LIST_prev(at), (link))

/** Unlink a link from the list. Only defined on double linked lists.
 * @param list list to be manipulated.
 * @param link to be unlinked. Must be part of the list. If the link is
 *  the lists anchor, nothing will happen.
 */
#define LIST_unlink(list, link) \
	((void)LIST_SelectD((list), unlink)((list), (link)))

/** Unlink first link of the list and return it.
 * @note if the list is empty, the list will still be empty but the anchor
 *  is returned. Be sure to check this condition.
 * @param list list to be shifted.
 * @return first link of the list or the anchor if the list was empty.
 */
#define LIST_shift(list) \
	LIST_SelectSD((list), shift)(list)

/** Unlink last link of the list and return it. Only defined on double linked
 *   lists.
 * @note if the list is empty, the list will still be empty but the anchor
 *  is returned. Be sure to check this condition.
 * @param list list to be popped.
 * @return last link of the list or the anchor if the list was empty.
 */
#define LIST_pop(list) \
	LIST_SelectD((list), pop)(list)

/** Insert a new link at the beginning of the list.
 * @param list list to be manipulated.
 * @param link link to be inserted at the beginning. Must not be part of another
 *  list.
 */
#define LIST_unshift(list, link) \
	LIST_SelectSD((list), unshift)((list), (link))

/** Insert a new link at the end of the list.
 * @param list list to be manipulated.
 * @param link link to be inserted at the end. Must not be part of another list.
 */
#define LIST_push(list, link) \
	LIST_SelectSD((list), push)((list), (link))

/** Separate the lists tail beginning from (including) a pivot into a new list
 *  and fix the counting (eager counting) for counted lists. Only defined on
 *  double linked lists.
 * @param list list to be separated. Will be cut (excluding) at the pivot.
 * @param newList list to hold the tail beginning (including) at the pivot. May
 *  be uninitialized.
 * @param pivot pivot link where the list is separated. Must be part of
 *  the source list.
 * @note if the pivot is the anchor, the source list will be left untouched and
 *  the new list will be cleared.
 * @note if the pivot is the head, the source list will be emptied and the
 *  destination list will hold all links.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
#define LIST_separateFrom(list, newList, pivot) \
	_Generic((list), struct LIST_ListD *: LIST_separateFromD, \
		struct LIST_ListDC *: LIST_separateFromFixDC)\
		((list), (newList), (pivot))

/** Separate the lists head beginning from (excluding) a pivot into a new list
 *  and fix the counting (eager counting) for counted lists. Only defined on
 *  double linked lists.
 * @param list list to be separated. Will be cut (including) at the pivot and
 *  include the tail only.
 * @param newList list to hold the head until (excluding) the pivot. May be
 *  uninitialized.
 * @param pivot pivot link where the list is separated. Must be part of
 *  the source list.
 * @note if the pivot is the anchor, the source list will be emptied and
 *  the destination list will hold all links.
 * @note if the pivot is the source lists head, the source list will be left
 *  untouched and the destination list is emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
#define LIST_separateUntil(list, newList, pivot) \
	_Generic((list), struct LIST_ListD *: LIST_separateUntilD, \
		struct LIST_ListDC *: LIST_separateUntilFixDC)\
		((list), (newList), (pivot))

/** Concatenate two lists by appending a source list to a destination list.
 * @param dst destination list. Will contain its old links followed by the
 *  links of the source list.
 * @param src source list. Will be emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
#define LIST_concatenatePush(dstList, srcList) \
	LIST_SelectSD((dstList), concatenatePush)((dstList), (srcList));

/** Concatenate two lists by prepending a source list to a destination list.
 * @param dst destination list. Will contain the links of the source list
 *  followed by its initial links.
 * @param src source list. Will be emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
#define LIST_concatenateUnshift(dstList, srcList) \
	LIST_SelectSD((dstList), concatenateUnshift)((dstList), (srcList));

/** Create list from a vector with a given member size.
 * @param list destination list. May be uninitialized.
 * @param vector pointer to elements of a vector.
 * @param member list member in the elements' structure.
 * @param n number of items of the vector.
 * @param elemSize size of one element in the vector.
 */
#define LIST_fromVectorSize(list, vector, member, n, elemSize) \
	LIST_SelectSD((list), fromVector)((list), (vector), \
		offsetof(__typeof__(*(vector)), member), n, (elemSize))
#else

/** Workaround: coerce single linked links to double linked links */
#define LIST_LinkS LIST_LinkD

/** Workaround: coerce single linked lists to double linked counted lists */
#define LIST_ListS LIST_ListDC

/** Workaround: coerce single linked counted lists to double linked counted
 * lists */
#define LIST_ListSC LIST_ListDC

/** Workaround: coerce double linked lists to double linked counted lists */
#define LIST_ListD LIST_ListDC

/** Initialize (statically) a double linked list.
 * @param name list name
 * @return initializer expression for the list
 */
#define LIST_ListD_INIT(name) LIST_ListDC_INIT(name)

/** Initialize (statically) a single linked list.
 * @param name list name
 * @return initializer expression for the list
 */
#define LIST_ListS_INIT(name) LIST_ListDC_INIT(name)

/** Initialize (statically) a single linked counted list.
 * @param name list name
 * @return initializer expression for the list
 */
#define LIST_ListSC_INIT(name) LIST_ListDC_INIT(name)

/** Initialize the list. Must be called before using the list,
 *   except for replacing operations, such as LIST_separateFrom().
 * @note Can also be used to clear the list. When clearing the list, be sure to
 *   have acess to the list in another way.
 * @param list list to be initialized.
 */
#define LIST_init(list) LIST_initDC(list)

/** Test whether the list is empty.
 * @param list list to be tested.
 * @return true iff list is empty.
 */
#define LIST_empty(list) LIST_emptyDC(list)

/** Get length of the list. Only defined on counted lists.
 * @param list the list.
 * @return number of links in the list.
 * @note if the list is a double linked list and is tainted, the links will be
 *  counted in O(n)
 */
#define LIST_length(list) LIST_lengthDC(list)

/** Get first link of the list.
 * @param list the list.
 * @return first link of the list.
 */
#define LIST_begin(list) LIST_beginDC(list)

/** Get last link of the list. Only defined on double linked lists.
 * @param list the list.
 * @return first link of the list.
 */
#define LIST_beginRev(list) LIST_beginRevDC(list)

/** Get anchor of the list.
 * @param list the list.
 * @return anchor of the list.
 */
#define LIST_end(list) LIST_endDC(list)

/** Get successor of a link.
 * @param link the link.
 * @return successor of the link.
 * @note if the link is the last link, the anchor will be returned.
 * @note if the link is the anchor, it will return the first link again.
 */
#define LIST_next(link) ((link)->next)

/** Get predecessor of a link. Only defined on links of double linked lists.
 * @param link the link.
 * @return successor of the link.
 * @note if the link is the first link, the anchor will be returned.
 * @note if the link is the anchor, it will return the last link again.
 */
#define LIST_prev(link) ((link)->prev)

/** Get n-th successor of a link.
 * @param link the link.
 * @return n-th successor of the link.
 * @note although the lists are circular, be sure to understand that the anchor
 *  is in between the last and the first link. Thus, when iterating after the
 *  end of the list, first the anchor will be taken and then the first link
 *  again. Best practice is to not iterate after the end of the list.
 */
#define LIST_forward(link, n) ({ \
	__typeof__(link) _link = (link); \
	__typeof__(n) _n = (n); \
	while (_n--) _link = LIST_next(_link); \
	_link; })

/** Get n-th predecessor of a link. Only defined on links of double linked
 *   lists.
 * @param link the link.
 * @return n-th predecessor of the link.
 * @note although the lists are circular, be sure to understand that the anchor
 *  is in between the last and the first link. Thus, when iterating before the
 *  begin of the list, first the anchor will be taken and then the last link
 *  again. Best practice is to not iterate before the beginning of the list.
 */
#define LIST_rewind(link, n) ({ \
	__typeof__(link) _link = (link); \
	__typeof__(n) _n = (n); \
	while (_n--) _link = LIST_prev(_link); \
	_link; })

/** Get n-th link of the list.
 * @param list the list.
 * @param n number of links to iterate forward.
 * @return n-th link of the list.
 * @note although the lists are circular, be sure to understand that the anchor
 *  is in between the last and the first link. Thus, when iterating after the
 *  end of the list, first the anchor will be taken and then the first link
 *  again. Best practice is that n is <= the length of the list.
 */
#define LIST_at(list, pos) \
	LIST_atDC((list), (pos))

/** Insert a new link after another link which is member of the list. Only
 *   defined on double linked lists.
 * @param at link before which the new link will be inserted. This link must
 *  be part of a list.
 * @param link link to be inserted after prev. Must not be part of another list.
 * @note this has another semantics than the LIST_insert* functions: they are
 *  given the predecessor of the new link, i.e. the new link will be inserted
 *  after the given link. This macro will insert the new link BEFORE the given
 *  one. If 'at' is the n-th link of the list, it will be the n+1-th link and
 *  'link' will be the new n-th link.
 */
#define LIST_insert(list, at, link) \
	LIST_insertDC((list), LIST_prev(at), (link))

/** Unlink a link from the list. Only defined on double linked lists.
 * @param list list to be manipulated.
 * @param link to be unlinked. Must be part of the list. If the link is
 *  the lists anchor, nothing will happen.
 */
#define LIST_unlink(list, link) \
	((void)LIST_unlinkDC((list), (link)))

/** Unlink first link of the list and return it.
 * @note if the list is empty, the list will still be empty but the anchor
 *  is returned. Be sure to check this condition.
 * @param list list to be shifted.
 * @return first link of the list or the anchor if the list was empty.
 */
#define LIST_shift(list) LIST_shiftDC(list)

/** Unlink last link of the list and return it. Only defined on double linked
 *   lists.
 * @note if the list is empty, the list will still be empty but the anchor
 *  is returned. Be sure to check this condition.
 * @param list list to be popped.
 * @return last link of the list or the anchor if the list was empty.
 */
#define LIST_pop(list) LIST_popDC(list)

/** Insert a new link at the beginning of the list.
 * @param list list to be manipulated.
 * @param link link to be inserted at the beginning. Must not be part of another
 *  list.
 */
#define LIST_unshift(list, link) \
	LIST_unshiftDC((list), (link))

/** Insert a new link at the end of the list.
 * @param list list to be manipulated.
 * @param link link to be inserted at the end. Must not be part of another list.
 */
#define LIST_push(list, link) \
	LIST_pushDC((list), (link))

/** Separate the lists tail beginning from (including) a pivot into a new list
 *  and fix the counting (eager counting) for counted lists. Only defined on
 *  double linked lists.
 * @param list list to be separated. Will be cut (excluding) at the pivot.
 * @param newList list to hold the tail beginning (including) at the pivot. May
 *  be uninitialized.
 * @param pivot pivot link where the list is separated. Must be part of
 *  the source list.
 * @note if the pivot is the anchor, the source list will be left untouched and
 *  the new list will be cleared.
 * @note if the pivot is the head, the source list will be emptied and the
 *  destination list will hold all links.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
#define LIST_separateFrom(list, newList, pivot) \
	LIST_separateFromDC((list), (newList), (pivot))

/** Separate the lists head beginning from (excluding) a pivot into a new list
 *  and fix the counting (eager counting) for counted lists. Only defined on
 *  double linked lists.
 * @param list list to be separated. Will be cut (including) at the pivot and
 *  include the tail only.
 * @param newList list to hold the head until (excluding) the pivot. May be
 *  uninitialized.
 * @param pivot pivot link where the list is separated. Must be part of
 *  the source list.
 * @note if the pivot is the anchor, the source list will be emptied and
 *  the destination list will hold all links.
 * @note if the pivot is the source lists head, the source list will be left
 *  untouched and the destination list is emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
#define LIST_separateUntil(list, newList, pivot) \
	LIST_separateUntilDC((list), (newList), (pivot))

/** Concatenate two lists by appending a source list to a destination list.
 * @param dst destination list. Will contain its old links followed by the
 *  links of the source list.
 * @param src source list. Will be emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
#define LIST_concatenatePush(dstList, srcList) \
	LIST_concatenatePushDC((dstList), (srcList));

/** Concatenate two lists by prepending a source list to a destination list.
 * @param dst destination list. Will contain the links of the source list
 *  followed by its initial links.
 * @param src source list. Will be emptied.
 * @note if the source and destination list are the same, the behavior is
 *  undefined.
 */
#define LIST_concatenateUnshift(dstList, srcList) \
	LIST_concatenateUnshiftDC((dstList), (srcList));


/** Create list from a vector with a given member size.
 * @param list destination list. May be uninitialized.
 * @param vector pointer to elements of a vector.
 * @param member list member in the elements' structure.
 * @param n number of items of the vector.
 * @param elemSize size of one element in the vector.
 */
#define LIST_fromVectorSize(list, vector, member, n, elemSize) \
	LIST_fromVectorDC((list), (vector), \
		offsetof(__typeof__(*(vector)), member), n, (elemSize))
#endif


/** Create list from a vector with a given member size.
 * @param list destination list. May be uninitialized.
 * @param vector pointer to elements of a vector.
 * @param member list member in the elements' structure.
 * @param n number of items of the vector.
 */
#define LIST_fromVector(list, vector, member, n) \
	LIST_fromVectorSize(list, vector, member, n, sizeof(*(vector)))

/** Iterate over a list (foreach control flow).
 * @param list list to iterate over.
 * @param link iterator.
 * @note you can use break inside the loop.
 * @note do not use unlink while iterating.
 */
#define LIST_foreach(list, link) \
	for (link = LIST_begin(list); link != LIST_end(list); \
		link = LIST_next(link))

/** Iterate over a list (foreach control flow) with the possibility to unlink
 *   the iterator link while iterating.
 * @param list list to iterate over.
 * @param link iterator.
 * @param n bogus iterator, used internally only. Do not use this in the loop.
 * @note you can use break inside the loop.
 */
#define LIST_foreachSafe(list, link, n) \
	for (link = LIST_begin(list), n = LIST_next(link); \
		link != LIST_end(list); link = n, n = LIST_next(link))

/** Iterate over a list (foreach control flow) in reverse.
 * @param list list to iterate over.
 * @param link iterator.
 * @note you can use break inside the loop.
 * @note do not use unlink while iterating.
 */
#define LIST_foreachRev(list, elem) \
	for (link = LIST_beginRev(list); link != LIST_end(list); \
			link = LIST_prev(link))

/** Iterate over a list (foreach control flow) in reverse with the possibility
 *   to unlink the iterator link while iterating.
 * @param list list to iterate over.
 * @param link iterator.
 * @note you can use break inside the loop.
 */
#define LIST_foreachRevSafe(list, elem, n) \
	for (link = LIST_beginRev(list), n = LIST_prev(link); \
		link != LIST_end(list); link = n, n = LIST_prev(link))

/** Iterate over a list (foreach control flow) with combined dereference of the
 *   link.
 * @param list list to iterate over.
 * @param item iterator (is also the item).
 * @note you can use break inside the loop.
 */
#define LIST_foreachItem(list, item, member) \
	for (item = LIST_item(LIST_begin(list), __typeof__(*item), member); \
		LIST_link(item, member) != LIST_end(list); \
		item = LIST_item(LIST_next(LIST_link(item, member)), __typeof__(*item), member))

/** Iterate over a list (foreach control flow) but begin right after the current
 *   (given) item with combined dereference of the link.
 * @param list list to iterate over.
 * @param item iterator (is also the item). Its successor is also the beginning
 *  of the iteration.
 * @note you can use break inside the loop.
 * @note this is meant to be a continuation point for foreachItem which was
 *  terminated with a break
 */
#define LIST_foreachItem_continue(list, item, member) \
	for (item = LIST_item(LIST_next(LIST_link(item, member)), __typeof__(*item), member); \
		LIST_link(item, member) != LIST_end(list); \
		item = LIST_item(LIST_next(LIST_link(item, member)), __typeof__(*item), member))

/** Iterate over a list (foreach control flow) in reverse with combined
 *   dereference of the link.
 * @param list list to iterate over.
 * @param item iterator (is also the item).
 * @note you can use break inside the loop.
 */
#define LIST_foreachItemRev(list, item, member) \
	for (item = LIST_item(LIST_beginRev(list), __typeof__(*item), member); \
		LIST_link(item, member) != LIST_end(list); \
		item = LIST_item(LIST_prev(LIST_link(item, member)), __typeof__(*item), member))

/** Iterate over a list (foreach control flow) in reverse but begin right before
 *   the current (given) item with combined dereference of the link.
 * @param list list to iterate over.
 * @param item iterator (is also the item). Its predecessor is also the
 *  beginning of the iteration.
 * @note you can use break inside the loop.
 * @note this is meant to be a continuation point for foreachItemRev which was
 *  terminated with a break
 */
#define LIST_foreachItemRev_continue(list, item, member) \
	for (item = LIST_item(LIST_prev(LIST_link(item, member)), __typeof__(*item), member); \
		LIST_link(item, member) != LIST_end(list); \
		item = LIST_item(LIST_prev(LIST_link(item, member)), __typeof__(*item), member))

#endif
