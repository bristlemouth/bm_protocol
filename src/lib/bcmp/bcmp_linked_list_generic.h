#pragma once

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"

typedef struct bcmp_ll_element_s {
  uint64_t node_id;
  void (*fp)(void*);
  bcmp_ll_element_s *next;
  bcmp_ll_element_s *prev;
} bcmp_ll_element_t;

class BCMP_Linked_List_Generic {
  public:

    // Constriuctors/destructors ***********************************************
    BCMP_Linked_List_Generic();
    ~BCMP_Linked_List_Generic();

    // Access functions ********************************************************
    int getSize();

    // Manipulation procedures *************************************************
    bool add(uint64_t node_id, void (*fp)(void*));
    bool remove(bcmp_ll_element_t *element);
    bcmp_ll_element_t* find(uint64_t node_id);

  private:

    bcmp_ll_element_t* newElement(uint64_t node_id, void (*fp)(void*));
    void freeElement(bcmp_ll_element_t **element_pointer);

    bcmp_ll_element_t *head;
    bcmp_ll_element_t *tail;
    int size;
    SemaphoreHandle_t mutex;
};
