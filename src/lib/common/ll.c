#include "ll.h"
#include <stddef.h>


/******************************************************************************
 * @brief Advances Node Cursor On Linked List
 *
 * @details Advances the cursor on an element of a linked list and
 *          retreives its data. This starts from the head. ll->cursor should be
 *          monitored to determine if the iterator is at the end of the list
 *          (ll->cursor == NULL).
 *
 * @param ll list to iterate cursor on node from
 * @param data data of node
 *
 * @return 0 on success
 * @return stderr on failure
 *****************************************************************************/
static int llAdvanceCursor(LL_t *ll, void **data);

/******************************************************************************
 * @brief Reset List Cursor
 *
 * @details Resets list cursor to begin iterative operations from the list's
 *          head. This should be used once the pop API has reached the end
 *          of the list.
 *
 * @param ll list to reset cursor
 *
 * @return 0 on success
 * @return stderr on failure
 *****************************************************************************/
static int llResetCursor(LL_t *ll);

int LLNodeAdd(LL_t *ll, LLNode_t *node)
{
    int ret = EIO;
    if (ll && node)
    {
        ret = 0;
        if (ll->head)
        {
            node->previous = ll->tail;
            ll->tail->next = node;
            ll->tail = node;
            ll->tail->next = NULL;
        }
        else
        {
            ll->head = ll->tail = node;
            ll->head->next = ll->head->previous = NULL;
        }
    }
    return ret;
}

int LLGetElement(LL_t *ll, uint32_t id, void **data)
{
    int ret = EIO;
    LLNode_t *current = NULL;
    if (ll && data)
    {
        current = ll->head;
        ret = ENODEV;
        while (current != NULL && current->id != id)
        {
            current = current->next;
        }
        if (current)
        {
            *data = current->data;
            ret = 0;
        }
    }
    return ret;
}

int LLRemove(LL_t *ll, uint32_t id)
{
    int ret = EIO;
    LLNode_t **current = NULL;
    if (ll)
    {
        current = &ll->head;
        ret = ENODEV;
        while (*current != NULL && (*current)->id != id)
        {
            current = &(*current)->next;
        }
        if (*current)
        {
            ret = 0;
            if (*current == ll->head)
            {
                ll->head = (*current)->next;
            }
            else
            {
                if (*current == ll->tail)
                {
                    ll->tail = (*current)->previous;
                }
                *current = (*current)->next;
            }
        }
    }
    return ret;
}

int LLTraverse(LL_t *ll, LLTraverseCb_t cb, void *arg)
{
    int ret = EIO;
    void *data = NULL;
    if (ll && cb)
    {
        llResetCursor(ll);
        do
        {
            ret = llAdvanceCursor(ll, &data);
            if (ret == 0)
            {
                ret = cb(data, arg);
            }
        }
        while (ll->cursor && ret == 0);
    }
    return ret;
}

static int llAdvanceCursor(LL_t *ll, void **data)
{
    int ret = EIO;
    if (ll && data)
    {
        ret = ENODEV;
        if (!ll->cursor)
        {
            ll->cursor = ll->head;
        }
        if (ll->cursor)
        {
            *data = ll->cursor->data;
            ll->cursor = ll->cursor->next;
            ret = 0;
        }
    }
    return ret;
}

static int llResetCursor(LL_t *ll)
{
    int ret = EIO;
    if (ll)
    {
        ll->cursor = NULL;
        ret = 0;
    }
    return ret;
}
