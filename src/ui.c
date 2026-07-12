#include "nine_top_ui.h"

#include <math.h>
#include <string.h>

double nine_top_percent(uint64_t used, uint64_t total) {
    if (total == 0) return NAN;
    double result = (double)used / (double)total * 100.0;
    if (result < 0) return 0;
    return result > 100 ? 100 : result;
}

nine_top_level nine_top_classify(double value, double warning, double critical) {
    if (!isfinite(value)) return NINE_TOP_UNAVAILABLE;
    if (value >= critical) return NINE_TOP_HIGH;
    if (value >= warning) return NINE_TOP_ELEVATED;
    return NINE_TOP_NORMAL;
}

void nine_top_fit(char *output, size_t capacity, const char *text, size_t width) {
    if (!output || capacity == 0) return;
    size_t target = width < capacity - 1 ? width : capacity - 1;
    size_t length = text ? strlen(text) : 0;
    size_t copied = length < target ? length : target;
    if (copied) memcpy(output, text, copied);
    if (target > copied) memset(output + copied, ' ', target - copied);
    output[target] = '\0';
}
