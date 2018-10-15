#include "esp_useful.h"

#include "time.h"

clock_t clock_ms()
{
    return (clock_t) (1000 * clock() / CLOCKS_PER_SEC);
}