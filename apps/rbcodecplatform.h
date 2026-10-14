#ifndef RBCODECPLATFORM_H_INCLUDED
#define RBCODECPLATFORM_H_INCLUDED
/* abs, atoi, labs, rand */
#include <stdlib.h>

bool tdspeed_alloc_buffers(int32_t **buffers, const int *buf_s, int nbuf);
void tdspeed_free_buffers(int32_t **buffers, int nbuf);

#endif

