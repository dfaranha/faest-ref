#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "vc_stream.h"
#include "random_oracle.h"
#include "compat.h"
#include "aes.h"
#include "instances.h"

#include <assert.h>
#include <string.h>

/*
unsigned int NumRec(unsigned int depth, const uint8_t* bi) {
  unsigned int out = 0;
  for (unsigned int i = 0; i < depth; i++) {
    out += ((unsigned int)bi[i]) << i;
  }
  return out;
}
*/

static void H0(const uint8_t* node, uint32_t lambda, const uint8_t* iv, uint8_t* sd, uint8_t* com) {
  const unsigned int lambda_bytes = lambda / 8;
  H0_context_t h0_ctx;
  H0_init(&h0_ctx, lambda);
  H0_update(&h0_ctx, node, lambda_bytes);
  H0_update(&h0_ctx, iv, IV_SIZE);
  H0_final(&h0_ctx, sd, lambda_bytes, com, (lambda_bytes * 2));
}

// index is the index i for (sd_i, com_i)
void get_sd_com(stream_vec_com_t* sVecCom, const uint8_t* iv, uint32_t lambda, unsigned int index, uint8_t* sd, uint8_t* com) {
  const unsigned int lambdaBytes = lambda / 8;

  uint8_t* children = alloca(lambdaBytes * 2);
  uint8_t* l_child = children;
  uint8_t* r_child = l_child + lambdaBytes;

  size_t lo = 0;
  size_t leaf_count = (1 << sVecCom->depth);
  size_t hi = leaf_count - 1;
  size_t center;

  // Find starting point from path memory
  size_t i = 0;
  if (sVecCom->path != NULL && sVecCom->index != sVecCom->depth) {
    for (; i < sVecCom->depth; i++) {
      center = (hi - lo) / 2 + lo;
      if (index <= center) { // Left
        if (sVecCom->index > center)
          break;
        hi = center;
      }
      else { // Right
        if (sVecCom->index < center + 1)
          break;
        lo = center + 1;
      }
    }
  }

  // Set starting node
  uint8_t* node;
  if (i > 0)
    node = sVecCom->path + (i - 1) * lambdaBytes;
  else
    node = sVecCom->rootKey;


  // Continue computing until leaf is reached
  for (; i < sVecCom->depth; i++) {
    prg(node, iv, children, lambda, lambdaBytes * 2);

    center = (hi - lo) / 2 + lo;
    if (index <= center) { // Left
      node = l_child;
      hi = center;
    }
    else { // Right
      node = r_child;
      lo = center + 1;
    }
    if (sVecCom->path != NULL)
      memcpy(sVecCom->path + i * lambdaBytes, node, lambdaBytes);
  }

  sVecCom->index = index;

  H0(node, lambda, iv, sd, com);
}

void stream_vector_commitment(const uint8_t* rootKey, uint32_t lambda, stream_vec_com_t* sVecCom, uint32_t depth) {
  const unsigned int lambdaBytes = lambda / 8;
  memcpy(sVecCom->rootKey, rootKey, lambdaBytes);
  sVecCom->depth = depth;
  sVecCom->index = depth; // Signals no path yet
}