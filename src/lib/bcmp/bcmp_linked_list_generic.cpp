#include "bcmp_linked_list_generic.h"

// create a new linked list
BCMP_Linked_List_Generic::BCMP_Linked_List_Generic() {
  head = NULL;
  tail = NULL;
  size = 0;
  mutex = xSemaphoreCreateMutex();
  configASSERT( mutex != NULL );
}

// free the linked list
BCMP_Linked_List_Generic::~BCMP_Linked_List_Generic() {
  bcmp_ll_element_t *element = head;
  while (element != NULL) {
    bcmp_ll_element_t *next = element->next;
    freeElement(&element);
    element = next;
  }
}

// create a new element
bcmp_ll_element_t* BCMP_Linked_List_Generic::newElement(uint64_t node_id, void (*fp)(void*)) {
  bcmp_ll_element_t *element = static_cast<bcmp_ll_element_t *>(pvPortMalloc(sizeof(bcmp_ll_element_t)));
  configASSERT(element != NULL);
  element->node_id = node_id;
  element->fp = fp;
  element->next = NULL;
  element->prev = NULL;
  return element;
}

// free the element
void BCMP_Linked_List_Generic::freeElement(bcmp_ll_element_t **element_pointer) {
  if (element_pointer != NULL && *element_pointer != NULL) {
    vPortFree(*element_pointer);
    *element_pointer = NULL;
  }
}

// Linked List Operations ***************************************************
int BCMP_Linked_List_Generic::getSize() {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdPASS) {
    int size_rval = size;
    xSemaphoreGive(mutex);
    return size_rval;
  } else {
    return -1;
  }
}

bool BCMP_Linked_List_Generic::add(uint64_t node_id, void (*fp)(void*)) {
  bool rval = false;
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdPASS) {
    bcmp_ll_element_t *element = newElement(node_id, fp);
    if (head == NULL && tail == NULL) {
      head = element;
      tail = element;
    } else {
      tail->next = element;
      tail = element;
    }
    size++;
    xSemaphoreGive(mutex);
    rval = true;
  }
  return rval;
}

bool BCMP_Linked_List_Generic::remove(bcmp_ll_element_t *element){
  bool rval = false;
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdPASS) {
    if (element == head) {
      head = element->next;
    }
    if (element == tail) {
      tail = element->prev;
    }
    if (element->prev != NULL) {
      element->prev->next = element->next;
    }
    if (element->next != NULL) {
      element->next->prev = element->prev;
    }
    freeElement(&element);
    size--;
    xSemaphoreGive(mutex);
    rval = true;
  }
  return rval;
}

bcmp_ll_element_t* BCMP_Linked_List_Generic::find(uint64_t node_id) {
  bcmp_ll_element_t *returned_element = NULL;
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdPASS) {
    bcmp_ll_element_t *element = head;
    while (element != NULL) {
      if (element->node_id == node_id) {
        returned_element = element;
        break;
      }
      element = element->next;
    }
    xSemaphoreGive(mutex);
  }
  return returned_element;
}
