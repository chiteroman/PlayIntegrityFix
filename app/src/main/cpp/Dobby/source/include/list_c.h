#pragma once

struct list_head {
  struct list_head *next;
  struct list_head *prev;
};
#define container_of(ptr, type, member)                                                                                \
  ({                                                                                                                   \
    const __typeof(((type *)0)->member) *__mptr = (ptr);                                                               \
    (type *)((char *)__mptr - offsetof(type, member));                                                                 \
  })

#define INIT_LIST_HEAD(ptr)                                                                                            \
  do {                                                                                                                 \
    (ptr)->next = (ptr);                                                                                               \
    (ptr)->prev = (ptr);                                                                                               \
  } while (0)

static inline int list_empty(struct list_head *head) {
  return head->next == head;
}

static void __list_add(struct list_head *new_node, struct list_head *prev, struct list_head *next) {
  next->prev = new_node;
  new_node->next = next;
  new_node->prev = prev;
  prev->next = new_node;
}

static inline void list_add(struct list_head *new_node, struct list_head *head) {
  __list_add(new_node, head, head->next);
}

static inline void __list_del(struct list_head *prev, struct list_head *next) {
  next->prev = prev;
  prev->next = next;
}

static inline void list_del(struct list_head *entry) {
  __list_del(entry->prev, entry->next);
  entry->next = NULL;
  entry->prev = NULL;
}

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) list_entry((ptr)->next, type, member)

#define list_next_entry(pos, member) list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_for_each_entry(pos, head, member)                                                                         \
  for (pos = list_first_entry(head, typeof(*pos), member); &pos->member != (head); pos = list_next_entry(pos, member))