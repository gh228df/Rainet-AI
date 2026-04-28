# RNAB Usage

The rnab library can also be directly integrated into an existing c/c++ project simply by including the `rnab.h` file.<br>
In case you're targeting Windows/Linux/Android all of the required helpers should already be defined! <br>

## Main Tunables

```c
#define RNAB_HOT_FUNCTION                  // sets attributes for the hot search functions
#define RNAB_BENCHMARK 1                   // exposes the rnab_benchmark()
#define RNAB_DEBUG 1                       // prints the timings for the calls
#define TABLE_SIZE (1u << 22)              // defines the transposition table size, 32 * TABLE_SIZE in bytes, 1024 by default
#define RNAB_REVCACHE 1                    // defines the symmetrical caching for debugging purposes
#define TRACK_CUTOFF_STATS 1               // tracks and prints cutoff stats per best section on each iteration
#define BRANCH_DEBUG 1                     // tracks cutoff rates per minimax branch
#define _printf(...)                       // OPTIONAL: printf interface
#define RNAB_MT 1                          // tries to use multithreading if possible
```

## Minimal Build

In case you're targeting an unsupported system, the following macros should be defined:

```c
#define RNAB_DEBUG 1                          // prints the timings for the calls
   #define TIME_TYPE                          // time for the variable which holds timestamp
   #define CLEAR_TT()                         // function to clear the board, similar to memset(board, 0, sizeof(board))
   #define get_time(var)                      // get time and store in var
   #define get_time_diff_millis(stop, start)  // compute the time difference between stop and start and output as uint64_t in milliseconds
   #define _printf(...)                       // printf interface
   or
   #define sys_write(fd, buf, len)            // used by the internal _printf

#define start_search_timer(t)                 // Starts search timer for t milliseconds, sets should_exit to true when time runs out
#define stop_search_timer()                   // Stops the search timer, should handle the edge cases. In case should_exit was set, stop_search_timer() wouldn't be called
#define CLEAR_TT()                            // function to clear the board, similar to memset(board, 0, sizeof(board))
```

## ESP-32 Build Example

```c
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include "esp_rom_sys.h"
#include "soc/timer_group_reg.h"
#include "esp_timer.h"
#include "esp_log.h"

static void search_timer_cb(void *arg);

static inline uint64_t _timer_us(void)
{
    WRITE_PERI_REG(TIMG_T0UPDATE_REG(0), 1);
    uint32_t lo = READ_PERI_REG(TIMG_T0LO_REG(0));
    uint32_t hi = READ_PERI_REG(TIMG_T0HI_REG(0));
    return ((uint64_t)hi << 32) | lo;
}

static esp_timer_handle_t search_timer = NULL;

#define start_search_timer(t)                                      \
    do                                                             \
    {                                                              \
        should_exit = false;                                       \
        if (t == UINT32_MAX)                                       \
            return;                                                \
                                                                   \
        const esp_timer_create_args_t timer_args = {               \
            .callback = search_timer_cb,                           \
            .arg = NULL,                                           \
            .name = "search_timer"};                               \
                                                                   \
        esp_timer_create(&timer_args, &search_timer);              \
        esp_timer_start_once(search_timer, (uint64_t)t * 1000ULL); \
    } while (0);

#define stop_search_timer()             \
    do                                  \
    {                                   \
        if (search_timer == NULL)       \
        {                               \
            return;                     \
        }                               \
        esp_timer_stop(search_timer);   \
        esp_timer_delete(search_timer); \
        search_timer = NULL;            \
    } while (0);

#define TIME_TYPE uint64_t
#define CLEAR_TT() __builtin_memset(table, 0, sizeof(table))
#define RNAB_HOT_FUNCTION __attribute__((section(".iram1.text")))
#define _printf(...) esp_rom_printf(__VA_ARGS__)
#define get_time(var) ((var) = _timer_us())
#define get_time_diff_millis(stop, start) (((stop) - (start)) / 1000ULL)
#define RNAB_BENCHMARK 1
#define RNAB_DEBUG 1

#define TABLE_SIZE 4096 // lets hope for the best

#include "./rnab_engine/rnab.h"

static void search_timer_cb(void *arg)
{
    should_exit = true;
}

void app_main(void)
{
    WRITE_PERI_REG(TIMG_T0CONFIG_REG(0),
                   (80UL << TIMG_T0_DIVIDER_S) |
                       TIMG_T0_INCREASE |
                       TIMG_T0_EN
    );
    WRITE_PERI_REG(TIMG_T0LOADLO_REG(0), 0);
    WRITE_PERI_REG(TIMG_T0LOADHI_REG(0), 0);
    WRITE_PERI_REG(TIMG_T0LOAD_REG(0), 1);

    rnab_benchmark();

    while (1)
    {
    };
}
```
