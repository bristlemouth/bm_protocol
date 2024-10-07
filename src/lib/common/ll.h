#ifndef _LL_COMMON_H_
#define _LL_COMMON_H_

#include <stdint.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LLNode_s
{
    struct LLNode_s *next;
    struct LLNode_s *previous;
    void *data;
    uint32_t id;
} LLNode_t;

typedef struct
{
    LLNode_t *head;
    LLNode_t *tail;
    LLNode_t *cursor;
} LL_t;

typedef int (*LLTraverseCb_t)(void *data, void *arg);

/******************************************************************************
 * @brief Add A Node To A Linked List
 *
 * @details Please ensure that the node artifacts are not destroyed during the
 *          use of the linked list. One way to ensure this is to make your
 *          variables statically allocated.
 *
 * @param ll list to add node to
 * @param node node to add to linked list
 *
 * @return 0 on success
 * @return stderr on failure
 *****************************************************************************/
int LLNodeAdd(LL_t *ll, LLNode_t *node);

/******************************************************************************
 * @brief Get Data Element From Linked List
 *
 * @details Obtain data at specified id of linked list
 *
 * @param ll list to add node to
 * @param id identifier of node to obtain data
 * @param data data at specified identifier
 *
 * @return 0 on success
 * @return stderr on failure
 *****************************************************************************/
int LLGetElement(LL_t *ll, uint32_t id, void **data);

/******************************************************************************
 * @brief Remove Node From Linked List
 *
 * @details Remove node at specified id in linked list
 *
 * @param ll list to delete node from
 * @param id identifier of node to delete
 *
 * @return 0 on success
 * @return stderr on failure
 *****************************************************************************/
int LLRemove(LL_t *ll, uint32_t id);

/******************************************************************************
 * @brief Traverse Linked List And Perform Callback Operation
 *
 * @details Traverses linked list and performs a callback operation on each
 *          node of the list.
 *
 * @param ll list to traverse
 * @param cb callback operation to perform
 * @param arg argument passed to callback function
 *
 * @return 0 on success
 * @return stderr on failure
 *****************************************************************************/
int LLTraverse(LL_t *ll, LLTraverseCb_t cb, void *arg);

#ifdef __cplusplus
}
#endif

#endif // _LL_H_
