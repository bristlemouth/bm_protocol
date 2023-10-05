#include "bcmp_linked_list_generic.h"


// SemaphoreHandle_t test_mutex = xSemaphoreCreateMutex();

// uint8_t *test_buffer = (uint8_t *)pvPortMalloc(100);

// create a new linked list
BCMP_Linked_List_Generic::BCMP_Linked_List_Generic() {
  head = NULL;
  tail = NULL;
  size = 0;
  // mutex = xSemaphoreCreateMutexStatic(&mutexBuffer);
  // configASSERT( mutex != NULL );
  mutex = xSemaphoreCreateMutex();
}

// free the linked list
BCMP_Linked_List_Generic::~BCMP_Linked_List_Generic() {
  bcmp_ll_node_t *node = head;
  while (node != NULL) {
    bcmp_ll_node_t *next = node->next;
    freeNode(&node);
    node = next;
  }
}

// create a new node
bcmp_ll_node_t* BCMP_Linked_List_Generic::newNode(uint64_t node_id, void (*fp)(void*)) {
  bcmp_ll_node_t *node = (bcmp_ll_node_t *)pvPortMalloc(sizeof(bcmp_ll_node_t));
  configASSERT(node != NULL);
  node->node_id = node_id;
  node->fp = fp;
  node->next = NULL;
  node->prev = NULL;
  return node;
}

// free the node
void BCMP_Linked_List_Generic::freeNode(bcmp_ll_node_t **node_pointer) {
  if (node_pointer != NULL && *node_pointer != NULL) {
    vPortFree(*node_pointer);
    *node_pointer = NULL;
  }
}

// Linked List Operations ***************************************************
int BCMP_Linked_List_Generic::getSize() {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdPASS) {
    int size = size;
    xSemaphoreGive(mutex);
    return size;
  } else {
    return -1;
  }
}

bool BCMP_Linked_List_Generic::add(uint64_t node_id, void (*fp)(void*)) {
  bool rval = false;
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdPASS) {
    bcmp_ll_node_t *node = newNode(node_id, fp);
    if (head == NULL && tail == NULL) {
      head = node;
      tail = node;
    } else {
      tail->next = node;
      tail = node;
    }
    size++;
    xSemaphoreGive(mutex);
    rval = true;
  }
  return rval;
}

bool BCMP_Linked_List_Generic::remove(bcmp_ll_node_t *node){
  bool rval = false;
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdPASS) {
    if (node == head) {
      head = node->next;
    }
    if (node == tail) {
      tail = node->prev;
    }
    if (node->prev != NULL) {
      node->prev->next = node->next;
    }
    if (node->next != NULL) {
      node->next->prev = node->prev;
    }
    freeNode(&node);
    size--;
    xSemaphoreGive(mutex);
    rval = true;
  }
  return rval;
}

bcmp_ll_node_t* BCMP_Linked_List_Generic::find(uint64_t node_id) {
  bcmp_ll_node_t *returned_node = NULL;
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdPASS) {
    bcmp_ll_node_t *node = head;
    while (node != NULL) {
      if (node->node_id == node_id) {
        returned_node = node;
        break;
      }
      node = node->next;
    }
    xSemaphoreGive(mutex);
  }
  return returned_node;
}
