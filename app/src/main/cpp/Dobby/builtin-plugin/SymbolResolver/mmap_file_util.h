#pragma once

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/mman.h>

class MmapFileManager {
  const char *file_;
  uint8_t *mmap_buffer_;
  size_t mmap_buffer_size_;

public:
  explicit MmapFileManager(const char *file) : file_(file), mmap_buffer_(nullptr) {
  }

  ~MmapFileManager() {
    if (mmap_buffer_) {
      munmap((void *)mmap_buffer_, mmap_buffer_size_);
    }
  }

  uint8_t *map() {
    size_t file_size = 0;
    {
      struct stat s;
      int rt = stat(file_, &s);
      if (rt != 0) {
        // printf("mmap %s failed\n", file_);
        return NULL;
      }
      file_size = s.st_size;
    }

    return map_options(file_size, 0);
  }

  uint8_t *map_options(size_t _size, off_t _off) {
    if (!mmap_buffer_) {
      int fd = open(file_, O_RDONLY, 0);
      if (fd < 0) {
        // printf("%s open failed\n", file_);
        return NULL;
      }

      // auto align
      auto mmap_buffer = (uint8_t *)mmap(0, _size, PROT_READ | PROT_WRITE, MAP_FILE | MAP_PRIVATE, fd, _off);
      if (mmap_buffer == MAP_FAILED) {
        // printf("mmap %s failed\n", file_);
        return NULL;
      }

      close(fd);

      mmap_buffer_ = mmap_buffer;
      mmap_buffer_size_ = _size;
    }
    return mmap_buffer_;
  }
};
