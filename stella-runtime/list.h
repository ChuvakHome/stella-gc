#ifndef STELLA_LINKED_LIST_H
#define STELLA_LINKED_LIST_H

#include <stdbool.h>
#include <stddef.h>

#define LINKED_LIST_FOR_EACH(listptr, list_node) for (list_node = (listptr)->head; list_node != NULL; list_node = linked_list_item_next(list_node))

struct linked_list_node;

typedef struct linked_list_node {
  void *data;
  struct linked_list_node *prev, *next;
} linked_list_node;

typedef struct {
  struct linked_list_node *head, *tail;
  size_t length;
} linked_list;

linked_list linked_list_create(void);

size_t linked_list_length(const linked_list *ll);

linked_list_node* linked_list_push(linked_list *ll, void *data);

linked_list_node* linked_list_pop(linked_list *ll);

void linked_list_destroy(linked_list *ll);

linked_list_node* linked_list_item_prev(const linked_list_node *p);

linked_list_node* linked_list_item_next(const linked_list_node *p);

#endif
