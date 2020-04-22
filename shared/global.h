#ifndef GLOBAL_H_202004061741
#define GLOBAL_H_202004061741

#include <stdbool.h>

#ifdef DEBUG_
#define DBG_PRINTF(...) printf(__VA_ARGS__)
#define DBG_FPRINTF(...) fprintf(__VA_ARGS__)
#else
#define DBG_PRINTF(...) do{}while(0)
#define DBG_FPRINTF(...) do{}while(0)
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#endif