#include "trie.hpp"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/resource.h>

using namespace Util;

struct timeval * difftimeval (struct timeval * d, struct timeval * t1, struct timeval * t2)
{
  d->tv_sec = t2->tv_sec - t1->tv_sec;
  d->tv_usec = t2->tv_usec - t1->tv_usec;
  if (d->tv_usec < 0) {
    d->tv_usec += 1000000;
    d->tv_sec -= 1;
    if (d->tv_sec < 0) {
      d->tv_sec = 0;
      d->tv_usec = 0;
    }
  }
  return d;
}

double timevaltoms(struct timeval * t)
{
  return ((double) (t->tv_sec)) * 1000. + ((double) t->tv_usec) / 1000.;
}


double timevaltous(struct timeval * t)
{
  return ((double) (t->tv_sec)) * 1000000. + ((double) t->tv_usec);
}

#ifndef TN
  #define TN 1024
#endif

int main(int argc, char * argv[]) {
  struct timeval real, user, sys;
  struct timeval before, after;
  struct rusage selfb, selfa;
  struct timezone dtz;

  typedef Trie<unsigned, TN> TTrie;

#if 0
  for (unsigned int n = 0; n < (unsigned) argc; n+= 10) {
    printf("%u ", n);

    TTrie trie = TTrie();
    {
      gettimeofday(&before, &dtz);
      getrusage(RUSAGE_SELF, &selfb);

      for (unsigned k = 0; k < n; k++) {
        assert(trie.insert(strlen(argv[k]), (const uint8_t *)argv[k], k, true) == TTrie::SUCCESS);
      }

      gettimeofday(&after, &dtz);
      getrusage(RUSAGE_SELF, &selfa);

      difftimeval(&real, &before, &after);
      difftimeval(&user, &selfb.ru_utime, &selfa.ru_utime);
      difftimeval(&sys, &selfb.ru_stime, &selfa.ru_stime);
      printf("%g %g %g ", timevaltoms(&real), timevaltoms(&user), timevaltoms(&sys));
    }

    {
    unsigned v;

      gettimeofday(&before, &dtz);
      getrusage(RUSAGE_SELF, &selfb);

      for (unsigned k = 0; k < n; k++) {
        assert(trie.find(strlen(argv[k]), (const uint8_t *)argv[k], v) == TTrie::SUCCESS);
      }

      gettimeofday(&after, &dtz);
      getrusage(RUSAGE_SELF, &selfa);

      difftimeval(&real, &before, &after);
      difftimeval(&user, &selfb.ru_utime, &selfa.ru_utime);
      difftimeval(&sys, &selfb.ru_stime, &selfa.ru_stime);
      printf("%g %g %g ", timevaltoms(&real), timevaltoms(&user), timevaltoms(&sys));

    }
    printf("\n");
  }
#else
  Trie<unsigned, TN> trie = Trie<unsigned, TN>();
  unsigned v;
  for (unsigned k = 0; k < (unsigned) argc; k++) {
    printf("%zu ", strlen(argv[k]));
    gettimeofday(&before, &dtz);
    getrusage(RUSAGE_SELF, &selfb);
    trie.insert(strlen(argv[k]), (const uint8_t *)argv[k], k, true);
    gettimeofday(&after, &dtz);
    getrusage(RUSAGE_SELF, &selfa);
    difftimeval(&real, &before, &after);
    difftimeval(&user, &selfb.ru_utime, &selfa.ru_utime);
    difftimeval(&sys, &selfb.ru_stime, &selfa.ru_stime);
    printf("%g %g %g ", timevaltous(&real), timevaltous(&user), timevaltous(&sys));

    gettimeofday(&before, &dtz);
    getrusage(RUSAGE_SELF, &selfb);
    trie.find(strlen(argv[k]), (const uint8_t *)argv[k], v);
    gettimeofday(&after, &dtz);
    getrusage(RUSAGE_SELF, &selfa);
    difftimeval(&real, &before, &after);
    difftimeval(&user, &selfb.ru_utime, &selfa.ru_utime);
    difftimeval(&sys, &selfb.ru_stime, &selfa.ru_stime);
    printf("%g %g %g ", timevaltous(&real), timevaltous(&user), timevaltous(&sys));

    printf("\n");
  }
#endif


  return 0;
}

