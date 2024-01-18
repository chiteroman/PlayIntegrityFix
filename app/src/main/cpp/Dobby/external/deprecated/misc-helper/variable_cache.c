#include "misc-helper/variable_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include "deprecated/unistd_helper.h"

#include <errno.h>

typedef struct queue_entry_t {
  struct queue_entry *next;
  struct queue_entry *prev;
} queue_entry_t;

/* TODO: add a property or attribute indicate not serialized */
typedef struct var_entry_t {
  queue_entry_t entry_;
  char key[128];
  uint64_t value;
} var_entry_t;

var_entry_t *root = NULL;

static var_entry_t *cache_get_entry_internal(const char *name) {
  var_entry_t *entry;
  entry = root;
  while (entry != NULL) {
    if (strcmp(name, entry->key) == 0) {
      return entry;
    }
    entry = (var_entry_t *)entry->entry_.next;
  }
  return NULL;
}

void cache_set(const char *name, uint64_t value) {
  var_entry_t *entry = cache_get_entry_internal(name);
  if (entry) {
    entry->value = value;
    return;
  }

  entry = (var_entry_t *)malloc(sizeof(var_entry_t));
  strcpy(entry->key, name);
  entry->value = value;

  entry->entry_.next = (struct queue_entry *)root;
  root = entry;
}

uint64_t cache_get(const char *name) {
  var_entry_t *entry = cache_get_entry_internal(name);
  if (entry) {
    return entry->value;
  }
  return 0;
}

typedef struct entry_block {
  int key_length;
  int value_length;
} entry_block_t;

int serialized_to_file(const char *filepath) {
  int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0660);
  if (fd == -1) {
    printf("open %s failed: %s\n", filepath, strerror(errno));
    return -1;
  }

  var_entry_t *entry;
  entry = root;
  while (entry != NULL) {
    entry_block_t block = {0};
    {
      block.key_length = strlen(entry->key) + 1;
      block.value_length = sizeof(uint64_t);
      write(fd, &block, sizeof(block));
    }

    write(fd, entry->key, block.key_length);
    write(fd, &entry->value, block.value_length);

    entry = (var_entry_t *)entry->entry_.next;
  }
  close(fd);
  return 0;
}

int unserialized_from_file(const char *filepath) {
  int fd = open(filepath, O_RDONLY);
  if (fd == -1) {
    printf("open %s failed: %s\n", filepath, strerror(errno));
    return -1;
  }

  entry_block_t block = {0};
  while (read(fd, &block, sizeof(block)) > 0) {
    char key[128] = {0};
    uint64_t value = 0;

    read(fd, (void *)&key, block.key_length);
    read(fd, (void *)&value, block.value_length);

    {
      var_entry_t *entry = (var_entry_t *)malloc(sizeof(var_entry_t));
      strcpy(entry->key, key);
      entry->value = value;

      entry->entry_.next = (struct queue_entry *)root;
      root = entry;
    }
  }

  return 0;
}
