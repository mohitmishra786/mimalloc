#include <mimalloc.h>
#include <stdio.h>

int main(void) {
  mi_theap_t* theap = mi_theap_get_default();
  void* p1 = mi_malloc_csize(24);
  void* p2 = mi_zalloc_csize(24);
  void* p3 = mi_theap_malloc_csize(theap, 24);
  void* p4 = mi_theap_zalloc_csize(theap, 24);
  printf("%p %p %p %p\n", p1, p2, p3, p4);
  mi_free_csize(p1, 24); mi_free_csize(p2, 24);
  mi_free_csize(p3, 24); mi_free_csize(p4, 24);
  return 0;
}
