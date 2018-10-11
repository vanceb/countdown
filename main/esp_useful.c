#include "esp_useful.h"

#include <time.h>

unsigned long clock_ms()
{
    return 1000 * clock() / CLOCKS_PER_SEC;
}