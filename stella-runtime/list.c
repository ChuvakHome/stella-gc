#include <stdlib.h>

#include "list.h"

linked_list linked_list_create(void) {
  linked_list ll = { .length = 0, .head = NULL, .tail = NULL };

  return ll;
}

linked_list_node* linked_list_push(linked_list *ll, void *data) {
  if (ll == NULL) { return NULL; }

  linked_list_node *new_node = malloc(sizeof(struct linked_list_node));

  if (new_node == NULL)
    return NULL;

  new_node->data = data;
  new_node->next = NULL;

  ll->length++;

  if (ll->head == NULL) {
    ll->head = ll->tail = new_node;

    new_node->prev = NULL;
  } else {
    new_node->prev = ll->tail;
    ll->tail->next = new_node;

    ll->tail = new_node;
  }

  return new_node;
}

linked_list_node* linked_list_pop(linked_list *ll) {
  if (ll == NULL || ll->head == NULL) { return NULL; }

  linked_list_node *node = ll->tail;

  if (ll->head == ll->tail) {
    ll->head = ll->tail = NULL;
  } else {
    struct linked_list_node *new_tail = node->prev;

    new_tail->next = NULL;
    ll->tail = new_tail;
  }

  free(node);

  --ll->length;

  return ll->tail;
}

void linked_list_destroy(linked_list *ll) {
  if (ll == NULL) { return; }

  linked_list_node *p = ll->head;

  while (p != NULL) {
    linked_list_node *tmp = p->next;
    free(p);

    ll->length--;
    p = tmp;
  }
}

size_t linked_list_length(const linked_list *ll) {
  return ll->length;
}

linked_list_node* linked_list_item_prev(const linked_list_node *p) {
  return p != NULL ? p->prev : NULL;
}

linked_list_node* linked_list_item_next(const linked_list_node *p) {
  return p != NULL ? p->next : NULL;
}
