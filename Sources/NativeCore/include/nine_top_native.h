#ifndef NINE_TOP_NATIVE_H
#define NINE_TOP_NATIVE_H

#include <stdint.h>

typedef struct {
    double cpu_temperature;
    double gpu_temperature;
    double battery_temperature;
    double cpu_usage;
    double gpu_usage;
    uint64_t memory_used;
    uint64_t memory_total;
    uint64_t swap_used;
    uint64_t swap_total;
} nine_top_metrics;

// Returns zero when the public system metrics were collected successfully.
// Individual unavailable temperature fields are reported as NAN.
int nine_top_read_metrics(nine_top_metrics *metrics, int include_battery);
void nine_top_install_signal_handlers(void);
int nine_top_should_terminate(void);

#endif
