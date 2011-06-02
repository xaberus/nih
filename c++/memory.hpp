#include <new>

#define JEMALLOC_MANGLE 1
#include <jemalloc/jemalloc.h>

namespace std {
    class bad_alloc;
    extern const nothrow_t nothrow;
}

void * operator new(size_t size) throw(std::bad_alloc) {
  void * tmp = jmalloc(size);
  if (!tmp)
    throw std::bad_alloc();
  return tmp;
}

void operator delete(void * ptr) throw() {
  jfree(ptr);
}

void * operator new[](size_t size) throw(std::bad_alloc) {
  void * tmp = jmalloc(size);
  if (!tmp)
    throw std::bad_alloc();
  return tmp;
}

void operator delete[](void * ptr) throw() {
  jfree(ptr);
}


