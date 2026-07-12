#ifndef NINE_TOP_UI_H
#define NINE_TOP_UI_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    NINE_TOP_UNAVAILABLE,
    NINE_TOP_NORMAL,
    NINE_TOP_ELEVATED,
    NINE_TOP_HIGH
} nine_top_level;

double nine_top_percent(uint64_t used, uint64_t total);
nine_top_level nine_top_classify(double value, double warning, double critical);
void nine_top_fit(char *output, size_t capacity, const char *text, size_t width);

#endif
