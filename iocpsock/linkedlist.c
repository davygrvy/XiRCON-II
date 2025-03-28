/* ----------------------------------------------------------------------
 *
 * LinkedList.c --
 *
 *	Linked list routines.
 *
 * ----------------------------------------------------------------------
 * RCS: @(#) $Id: linkedlist.c,v 1.4 2006/09/13 03:12:04 davygrvy Exp $
 * ----------------------------------------------------------------------
 */

#include "iocpsockInt.h"

/*
 * compare and swap functions
 */

#if 0
#if !defined(_MSC_VER)
static inline char CAS (volatile void * addr, volatile void * value, void * newvalue) 
{
    register char ret;
    __asm__ __volatile__ (
	"# CAS \n\t"
	"lock ; cmpxchg %2, (%1) \n\t"
	"sete %0                 \n\t"
	:"=a" (ret)
	:"c" (addr), "d" (newvalue), "a" (value)
    );
    return ret;
}

static inline char CAS2 (volatile void * addr, volatile void * v1, volatile long v2, void * n1, long n2) 
{
    register char ret;
    __asm__ __volatile__ (
	"# CAS2 \n\t"
	"lock ;  cmpxchg8b (%1) \n\t"
	"sete %0                \n\t"
	:"=a" (ret)
	:"D" (addr), "d" (v2), "a" (v1), "b" (n1), "c" (n2)
    );
    return ret;
}
#else
static __inline char CAS (volatile void * addr, volatile void * value, void * newvalue) 
{
    register char c;
    __asm {
	push	ebx
	push	esi
	mov	esi, addr
	mov	eax, value
	mov	ebx, newvalue
	lock	cmpxchg dword ptr [esi], ebx
	sete	c
	pop	esi
	pop	ebx
    }
    return c;
}

static __inline char CAS2 (volatile void * addr, volatile void * v1, volatile long v2, void * n1, long n2) 
{
    register char c;
    __asm {
	push	ebx
	push	ecx
	push	edx
	push	esi
	mov	esi, addr
	mov	eax, v1
	mov	ebx, n1
	mov	ecx, n2
	mov	edx, v2
	lock    cmpxchg8b qword ptr [esi]
	sete	c
	pop	esi
	pop	edx
	pop	ecx
	pop	ebx
    }
    return c;
}
#endif
#endif //0


/* Bitmask macros. */
#define mask_a( mask, val ) if ( ( mask & val ) != val ) { mask |= val; }
#define mask_d( mask, val ) if ( ( mask & val ) == val ) { mask &= ~(val); }
#define mask_y( mask, val ) ( mask & val ) == val
#define mask_n( mask, val ) ( mask & val ) != val


/*
 *----------------------------------------------------------------------
 *
 * IocpLLCreate --
 *
 *	Creates a linked list.
 *
 * Results:
 *	pointer to the new one or NULL for error.
 *
 * Side effects:
 *	None known.
 *
 *----------------------------------------------------------------------
 */

LPLLIST
IocpLLCreate (void)
{   
    LPLLIST ll;
    
    /* Alloc a linked list. */
    if (!(ll = IocpAlloc(sizeof(LLIST)))) {
	return NULL;
    }
    if (!InitializeCriticalSectionAndSpinCount(&ll->lock, 4000)) {
	IocpFree(ll);
	return NULL;
    }
    ll->haveData = CreateEvent(NULL, TRUE, FALSE, NULL);  /* manual reset */
    if (ll->haveData == INVALID_HANDLE_VALUE) {
	DeleteCriticalSection(&ll->lock);
	IocpFree(ll);
	return NULL;
    }
    ll->back = ll->front = 0L;
    ll->lCount = 0;
    return ll;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLDestroy --
 *
 *	Destroys a linked list.
 *
 * Results:
 *	Same as HeapFree.
 *
 * Side effects:
 *	Nodes aren't destroyed.
 *
 *----------------------------------------------------------------------
 */

BOOL 
IocpLLDestroy (
    LPLLIST ll)
{
    if (!ll) {
	return FALSE;
    }
    DeleteCriticalSection(&ll->lock);
    CloseHandle(ll->haveData);
    return IocpFree(ll);
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPushFront --
 *
 *	Adds an item to the end of the list.
 *
 * Results:
 *	The node.
 *
 * Side effects:
 *	Will create a new node, if not given one.
 *
 *----------------------------------------------------------------------
 */

LPLLNODE 
IocpLLPushBack(
    LPLLIST ll,
    LPVOID lpItem,
    LPLLNODE pnode,
    DWORD dwState)
{
    LPLLNODE tmp;

    if (!ll) {
	return NULL;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!pnode) {
	pnode = IocpAlloc(sizeof(LLNODE));
    }
    if (!pnode) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return NULL;
    }
    pnode->lpItem = lpItem;
    if (!ll->front && !ll->back) {
	ll->front = pnode;
	ll->back = pnode;
    } else {
	ll->back->next = pnode;
	tmp = ll->back;
	ll->back = pnode;
	ll->back->prev = tmp;
    }
    ll->lCount++;
    pnode->ll = ll;
    SetEvent(ll->haveData);
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    return pnode;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPushFront --
 *
 *	Adds an item to the front of the list.
 *
 * Results:
 *	The node.
 *
 * Side effects:
 *	Will create a new node, if not given one.
 *
 *----------------------------------------------------------------------
 */

LPLLNODE 
IocpLLPushFront(
    LPLLIST ll,
    LPVOID lpItem,
    LPLLNODE pnode,
    DWORD dwState)
{
    LPLLNODE tmp;

    if (!ll) {
	return NULL;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!pnode) {
	pnode = IocpAlloc(sizeof(LLNODE));
    }
    if (!pnode) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return NULL;
    }
    pnode->lpItem = lpItem;
    if (!ll->front && !ll->back) {
	ll->front = pnode;
	ll->back = pnode;
    } else {
	ll->front->prev = pnode;
	tmp = ll->front;
	ll->front = pnode;
	ll->front->next = tmp;
    }
    ll->lCount++;
    pnode->ll = ll;
    SetEvent(ll->haveData);
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    return pnode;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPopAll --
 *
 *	Removes all items from the list.
 *
 * Results:
 *	TRUE if something was popped or FALSE if nothing was poppable.
 *
 * Side effects:
 *	Won't free the node(s) with IOCP_LL_NODESTROY in the state arg.
 *
 *----------------------------------------------------------------------
 */

BOOL 
IocpLLPopAll(
    LPLLIST ll,
    LPLLNODE snode,
    DWORD dwState)
{
    LPLLNODE tmp1, tmp2;

    if (!ll) {
	return FALSE;
    }
    if (snode && snode->ll) {
	ll = snode->ll;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!ll->front && ! ll->back || ll->lCount <= 0) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return FALSE;
    }
    tmp1 = ll->front;
    if (snode) {
	tmp1 = snode;
    }
    while(tmp1) {
	tmp2 = tmp1->next;
	/* Delete (or blank) the node and decrement the counter. */
        if (mask_n(dwState, IOCP_LL_NODESTROY)) {
	    IocpLLNodeDestroy(tmp1);
	} else {
	    tmp1->ll = NULL;
	    tmp1->next = NULL; 
            tmp1->prev = NULL;
	}
        ll->lCount--;
	tmp1 = tmp2;
    }

    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    
    return TRUE;
}


BOOL 
IocpLLPopAllCompare(
    LPLLIST ll,
    LPVOID lpItem,
    DWORD dwState)
{
    LPLLNODE tmp1, tmp2;

    if (!ll) {
	return FALSE;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!ll->front && !ll->back || ll->lCount <= 0) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return FALSE;
    }
    tmp1 = ll->front;
    while(tmp1) {
	tmp2 = tmp1->next;
	if (tmp1->lpItem == lpItem) {
	    IocpLLPop(tmp1, IOCP_LL_NOLOCK | dwState);
	}
	tmp1 = tmp2;
    }
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    
    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPop --
 *
 *	Removes an item from the list.
 *
 * Results:
 *	TRUE if something was popped or FALSE if nothing was poppable.
 *
 * Side effects:
 *	Won't free the node with IOCP_LL_NODESTROY in the state arg.
 *
 *----------------------------------------------------------------------
 */

BOOL 
IocpLLPop(
    LPLLNODE node,
    DWORD dwState)
{
    LPLLIST ll;
    LPLLNODE prev, next;

    //Ready the node
    if (!node || !node->ll) {
	return FALSE;
    }
    ll = node->ll;
    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	EnterCriticalSection(&ll->lock);
    }
    if (!ll->front && !ll->back || ll->lCount <= 0) {
	if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	    LeaveCriticalSection(&ll->lock);
	}
	return FALSE;
    }
    prev = node->prev;
    next = node->next;

    /* Check for only item. */
    if (!prev & !next) {
	ll->front = NULL;
	ll->back = NULL;
    /* Check for front of list. */
    } else if (!prev && next) {
	next->prev = NULL;
	ll->front = next;
    /* Check for back of list. */
    } else if (prev && !next) {
	prev->next = NULL;
	ll->back = prev;
    /* Check for middle of list. */
    } else if (prev && next) {
	next->prev = prev;
	prev->next = next;
    }

    /* Delete the node when IOCP_LL_NODESTROY is not specified. */
    if (mask_n(dwState, IOCP_LL_NODESTROY)) {
	IocpLLNodeDestroy(node);
    } else {
	node->ll = NULL;
	node->next = NULL; 
        node->prev = NULL;
    }
    ll->lCount--;
    if (ll->lCount <= 0) {
	ll->front = NULL;
	ll->back = NULL;
    }

    if (mask_n(dwState, IOCP_LL_NOLOCK)) {
	LeaveCriticalSection(&ll->lock);
    }
    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLNodeDestroy --
 *
 *	Destroys a node.
 *
 * Results:
 *	Same as HeapFree.
 *
 * Side effects:
 *	memory returns to the system.
 *
 *----------------------------------------------------------------------
 */

BOOL
IocpLLNodeDestroy (LPLLNODE node)
{
    return IocpFree(node);
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPopBack --
 *
 *	Removes the item at the back of the list.
 *
 * Results:
 *	The item stored in the node at the front or NULL for none.
 *
 * Side effects:
 *	Won't free the node with IOCP_LL_NODESTROY in the state arg.
 *
 *----------------------------------------------------------------------
 */

LPVOID
IocpLLPopBack(
    LPLLIST ll,
    DWORD dwState,
    DWORD timeout)
{
    LPLLNODE tmp;
    LPVOID data;

    if (!ll) {
	return NULL;
    }
    EnterCriticalSection(&ll->lock);
    if (!ll->lCount) {
	if (timeout) {
	    DWORD dwWait;
	    ResetEvent(ll->haveData);
	    LeaveCriticalSection(&ll->lock);
	    dwWait = WaitForSingleObject(ll->haveData, timeout);
	    if (dwWait == WAIT_OBJECT_0) {
		/* wait succedded, fall through and remove one. */
		EnterCriticalSection(&ll->lock);
	    } else {
		/* wait failed */
		return NULL;
	    }
	} else {
	    LeaveCriticalSection(&ll->lock);
	    return NULL;
	}
    }
    tmp = ll->back;
    data = tmp->lpItem;
    IocpLLPop(tmp, IOCP_LL_NOLOCK | dwState);
    LeaveCriticalSection(&ll->lock);
    return data;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLPopFront --
 *
 *	Removes the item at the front of the list.
 *
 * Results:
 *	The item stored in the node at the front or NULL for none.
 *
 * Side effects:
 *	Won't free the node with IOCP_LL_NODESTROY in the state arg.
 *
 *----------------------------------------------------------------------
 */

LPVOID
IocpLLPopFront(
    LPLLIST ll,
    DWORD dwState,
    DWORD timeout)
{
    LPLLNODE tmp;
    LPVOID data;

    if (!ll) {
	return NULL;
    }
    EnterCriticalSection(&ll->lock);
    if (!ll->lCount) {
	if (timeout) {
	    DWORD dwWait;
	    ResetEvent(ll->haveData);
	    LeaveCriticalSection(&ll->lock);
	    dwWait = WaitForSingleObject(ll->haveData, timeout);
	    if (dwWait == WAIT_OBJECT_0) {
		/* wait succedded, fall through and remove one. */
		EnterCriticalSection(&ll->lock);
	    } else {
		/* wait failed */
		return NULL;
	    }
	} else {
	    LeaveCriticalSection(&ll->lock);
	    return NULL;
	}
    }
    tmp = ll->front;
    data = tmp->lpItem;
    IocpLLPop(tmp, IOCP_LL_NOLOCK | dwState);
    LeaveCriticalSection(&ll->lock);
    return data;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLIsNotEmpty --
 *
 *	self explanatory.
 *
 * Results:
 *	Boolean for if the linked-list has entries.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

BOOL
IocpLLIsNotEmpty (LPLLIST ll)
{
    BOOL b;
    if (!ll) {
	return FALSE;
    }
    EnterCriticalSection(&ll->lock);
    b = (ll->lCount != 0);
    LeaveCriticalSection(&ll->lock);
    return b;
}

/*
 *----------------------------------------------------------------------
 *
 * IocpLLGetCount --
 *
 *	How many nodes are on the list?
 *
 * Results:
 *	Count of entries.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

SIZE_T
IocpLLGetCount (LPLLIST ll)
{
    SIZE_T c;
    if (!ll) {
	return 0;
    }
    EnterCriticalSection(&ll->lock);
    c = ll->lCount;
    LeaveCriticalSection(&ll->lock);
    return c;
}
