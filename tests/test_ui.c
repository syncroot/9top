#include "nine_top_ui.h"

#include <assert.h>
#include <math.h>
#include <string.h>

int main(void) {
    assert(nine_top_percent(4, 8) == 50.0);
    assert(isnan(nine_top_percent(1, 0)));
    assert(nine_top_percent(12, 10) == 100.0);

    assert(nine_top_classify(NAN, 70, 90) == NINE_TOP_UNAVAILABLE);
    assert(nine_top_classify(20, 70, 90) == NINE_TOP_NORMAL);
    assert(nine_top_classify(75, 70, 90) == NINE_TOP_ELEVATED);
    assert(nine_top_classify(95, 70, 90) == NINE_TOP_HIGH);

    char output[5];
    nine_top_fit(output, sizeof(output), "abcdefgh", 4);
    assert(strcmp(output, "abcd") == 0);
    nine_top_fit(output, sizeof(output), "ab", 4);
    assert(strcmp(output, "ab  ") == 0);
    return 0;
}
