#pragma once

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"

typedef struct bcmp_ll_node_s {
  uint64_t node_id;
  void (*fp)(void*);
  bcmp_ll_node_s *next;
  bcmp_ll_node_s *prev;
} bcmp_ll_node_t;

class BCMP_Linked_List_Generic {
  public:

    BCMP_Linked_List_Generic();
    ~BCMP_Linked_List_Generic();

    // Access functions ********************************************************
    int getSize();

    // Manipulation procedures *************************************************
    bool add(uint64_t node_id, void (*fp)(void*));
    bool remove(bcmp_ll_node_t *node);
    bcmp_ll_node_t* find(uint64_t node_id);

  private:

    bcmp_ll_node_t* newNode(uint64_t node_id, void (*fp)(void*));
    void freeNode(bcmp_ll_node_t **node_pointer);

    bcmp_ll_node_t *head;
    bcmp_ll_node_t *tail;
    int size;
    SemaphoreHandle_t mutex;
    StaticSemaphore_t mutexBuffer;
};
