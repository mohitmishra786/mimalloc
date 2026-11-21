// API contract edge cases (fork-only test harness, not for upstream)
#include <mimalloc.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

static int fails = 0;
#define CHECK(name, cond) do { if (!(cond)) { fails++; printf("FAIL %-55s (errno=%d) line %d\n", name, errno, __LINE__); } } while(0)
static int all_zero(const void* p, size_t n){ const unsigned char*q=(const unsigned char*)p; for(size_t i=0;i<n;i++) if(q[i]) return 0; return 1; }

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  void* p; int r;
  errno=0; p = mi_calloc(SIZE_MAX/2, 4);        CHECK("calloc overflow", p==NULL && errno==ENOMEM);
  errno=0; p = mi_mallocn(SIZE_MAX/2, 4);       CHECK("mallocn overflow", p==NULL && errno==ENOMEM);
  errno=0; p = mi_malloc(SIZE_MAX);             CHECK("malloc SIZE_MAX", p==NULL && errno==ENOMEM);
  errno=0; p = mi_malloc((size_t)PTRDIFF_MAX+1);CHECK("malloc PTRDIFF_MAX+1", p==NULL && errno==ENOMEM);
  { void* q = mi_malloc(10); errno=0; p = mi_reallocn(q, SIZE_MAX/2, 4); CHECK("reallocn overflow", p==NULL && errno==ENOMEM); mi_free(q); }
  { void* q = mi_malloc(10); errno=0; p = mi_realloc(q, SIZE_MAX); CHECK("realloc SIZE_MAX", p==NULL && errno==ENOMEM); mi_free(q); }
  { void* q = mi_malloc(10); errno=0; p = mi_recalloc(q, SIZE_MAX/2, 4); CHECK("recalloc overflow", p==NULL && errno==ENOMEM); mi_free(q); }
  { void* q = mi_malloc(10); errno=0; p = mi_reallocarray(q, SIZE_MAX/2, 4); CHECK("reallocarray overflow", p==NULL && errno==ENOMEM); mi_free(q); }
  { void* q = mi_malloc(10); errno=0; r = mi_reallocarr(&q, SIZE_MAX/2, 4); CHECK("reallocarr overflow", r==EOVERFLOW && q!=NULL); mi_free(q); }
  errno=0; p = mi_calloc_aligned(SIZE_MAX/2, 4, 64);   CHECK("calloc_aligned overflow", p==NULL && errno==ENOMEM);
  errno=0; p = mi_malloc_aligned(SIZE_MAX-100, 64);    CHECK("malloc_aligned huge", p==NULL && errno==ENOMEM);
  errno=0; p = mi_malloc_aligned(16, 0);   CHECK("align 0", p==NULL);
  errno=0; p = mi_malloc_aligned(16, 3);   CHECK("align 3", p==NULL);
  for (size_t a = 1; a <= ((size_t)1<<27); a <<= 1) {
    for (size_t s = 0; s <= 3*a && s < ((size_t)1<<26); s = (s==0?1:s*3)) {
      void* q = mi_malloc_aligned(s, a);
      if (q==NULL || ((uintptr_t)q % a)!=0 || mi_usable_size(q) < s) { fails++; printf("FAIL malloc_aligned(%zu,%zu) -> %p\n", s, a, q); }
      if (q) memset(q, 1, s);
      void* z = mi_zalloc_aligned(s, a);
      if (z==NULL || ((uintptr_t)z % a)!=0 || !all_zero(z,s)) { fails++; printf("FAIL zalloc_aligned(%zu,%zu)\n", s, a); }
      mi_free(q); mi_free(z);
    }
  }
  for (size_t a = 8; a <= 65536; a <<= 1) {
    size_t offs[] = {0, 1, 8, 16, a/2, a-1, a, a+8, 3*a};
    for (size_t i=0;i<sizeof(offs)/sizeof(offs[0]);i++) {
      size_t o = offs[i]; size_t s = o + 32;
      char* q = (char*)mi_malloc_aligned_at(s, a, o);
      if (q==NULL || (((uintptr_t)q + o) % a)!=0) { fails++; printf("FAIL malloc_aligned_at(%zu,%zu,%zu)\n", s,a,o); continue; }
      memset(q, 3, s);
      char* q2 = (char*)mi_realloc_aligned_at(q, s*5, a, o);
      if (q2==NULL || (((uintptr_t)q2 + o) % a)!=0 || q2[s-1]!=3) { fails++; printf("FAIL realloc_aligned_at(%zu,%zu,%zu)\n", s*5,a,o); mi_free(q); continue; }
      mi_free(q2);
    }
  }
  { char* q = (char*)mi_malloc_aligned(100, 256); memset(q, 7, 100);
    char* q2 = (char*)mi_realloc_aligned(q, 100000, 256);
    CHECK("realloc_aligned grow", q2 && ((uintptr_t)q2%256)==0 && q2[99]==7);
    char* q3 = (char*)mi_realloc_aligned(q2, 10, 4096);
    CHECK("realloc_aligned more align", q3 && ((uintptr_t)q3%4096)==0 && q3[9]==7);
    mi_free(q3); }
  for (size_t s0 = 1; s0 < 1000000; s0 *= 7) {
    unsigned char* q = (unsigned char*)mi_zalloc(s0); memset(q, 0xAB, s0);
    size_t s1 = s0*3+5;
    unsigned char* q2 = (unsigned char*)mi_rezalloc(q, s1);
    if (!q2 || q2[s0-1]!=0xAB || !all_zero(q2+s0, s1-s0)) { fails++; printf("FAIL rezalloc %zu->%zu\n", s0, s1); }
    unsigned char* q3 = (unsigned char*)mi_rezalloc(q2, s0/2+1);
    unsigned char* q4 = (unsigned char*)mi_rezalloc(q3, s1*2);
    if (!q4 || !all_zero(q4 + s0/2+1, s1*2 - (s0/2+1))) { fails++; printf("FAIL rezalloc shrink-grow %zu\n", s0); }
    mi_free(q4);
  }
  for (size_t s0 = 1; s0 < 1000000; s0 *= 7) {
    unsigned char* q = (unsigned char*)mi_zalloc_aligned(s0, 64); memset(q, 0xAB, s0);
    size_t s1 = s0*3+5;
    unsigned char* q2 = (unsigned char*)mi_rezalloc_aligned(q, s1, 64);
    if (!q2 || ((uintptr_t)q2 % 64) || q2[s0-1]!=0xAB || !all_zero(q2+s0, s1-s0)) { fails++; printf("FAIL rezalloc_aligned %zu->%zu\n", s0, s1); }
    unsigned char* q3 = (unsigned char*)mi_rezalloc_aligned(q2, s0/2+1, 64);
    unsigned char* q4 = (unsigned char*)mi_rezalloc_aligned(q3, s1*2, 64);
    if (!q4 || !all_zero(q4 + s0/2+1, s1*2 - (s0/2+1))) { fails++; printf("FAIL rezalloc_aligned shrink-grow %zu\n", s0); }
    mi_free(q4);
  }
  for (size_t n0 = 1; n0 < 100000; n0 *= 5) {
    int* q = (int*)mi_recalloc(NULL, n0, sizeof(int)); for (size_t i=0;i<n0;i++) q[i] = 0x11111111;
    int* q2 = (int*)mi_recalloc(q, n0*3, sizeof(int));
    if (!q2 || q2[n0-1]!=0x11111111 || !all_zero(q2+n0, 2*n0*sizeof(int))) { fails++; printf("FAIL recalloc %zu\n", n0); }
    mi_free(q2);
  }
  { void* q = mi_malloc(10); void* q2 = mi_realloc(q, 0); CHECK("realloc(p,0) non-NULL", q2!=NULL); mi_free(q2); }
  { char* s = mi_strndup("hello", 3); CHECK("strndup", s && strcmp(s,"hel")==0); mi_free(s); }
  { char* s = mi_strndup("hello", 100); CHECK("strndup n>len", s && strcmp(s,"hello")==0); mi_free(s); }
  { wchar_t* w = mi_wcsdup(L"abc"); CHECK("wcsdup", w && wcscmp(w,L"abc")==0); mi_free(w); }
  { void* q = (void*)0x1234; r = mi_posix_memalign(&q, 3, 10); CHECK("posix_memalign 3", r==EINVAL && q==(void*)0x1234); }
  { void* q = (void*)0x1234; r = mi_posix_memalign(&q, 64, SIZE_MAX-100); CHECK("posix_memalign huge", r==ENOMEM && q==(void*)0x1234); }
  { void* q = NULL; r = mi_posix_memalign(&q, (size_t)1<<20, 100); CHECK("posix_memalign 1MiB", r==0 && ((uintptr_t)q % (1<<20))==0); mi_free(q); }
  { void* q = mi_pvalloc(1); CHECK("pvalloc(1)", q && ((uintptr_t)q % 4096)==0); mi_free(q); }
  { errno=0; void* q = mi_pvalloc(SIZE_MAX-10); CHECK("pvalloc huge", q==NULL && errno==ENOMEM); }
  CHECK("usable_size(NULL)", mi_usable_size(NULL)==0);
  for (size_t s = 0; s < (1u<<26); s = s*2+1) {
    void* q = mi_malloc(s); size_t g = mi_good_size(s); size_t u = mi_usable_size(q);
    if (q==NULL || u < s || g < s) { fails++; printf("FAIL size %zu good=%zu usable=%zu\n", s, g, u); }
    if (q) memset(q, 0x5a, u);
    mi_free(q);
  }
  { mi_heap_t* h = mi_heap_new(); void* q = mi_heap_malloc(h, 100); CHECK("heap_contains", mi_heap_contains(h, q) && mi_heap_of(q)==h);
    errno=0; void* q2 = mi_heap_calloc(h, SIZE_MAX/2, 4); CHECK("heap_calloc overflow", q2==NULL && errno==ENOMEM);
    char* q4 = mi_heap_strndup(h, "abcdef", 2); CHECK("heap_strndup", q4 && strcmp(q4,"ab")==0);
    mi_heap_destroy(h); }
  printf("%d failures\n", fails);
  return fails != 0;
}
