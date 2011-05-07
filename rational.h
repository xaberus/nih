
#include <stdint.h>
#include "err.h"

/* small rational */

struct sr {
  int16_t num;
  uint16_t den;
};

typedef struct sr sr_t;

err_t sr_set(sr_t * rat, int16_t num, uint16_t den);

err_t sr_add(const sr_t * a, const sr_t * b, sr_t * result);
err_t sr_sub(const sr_t * a, const sr_t * b, sr_t * result);
err_t sr_mul(const sr_t * a, const sr_t * b, sr_t * result);
err_t sr_div(const sr_t * a, const sr_t * b, sr_t * result);
