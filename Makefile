CC := clang
CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -arch arm64 -mmacosx-version-min=14.0
LDFLAGS := -Wl,-pie,-dead_strip
LDLIBS := -framework IOKit -framework CoreFoundation
BUILD_DIR := build
SOURCES := src/main.c src/metrics.c src/ui.c

.PHONY: all release test sanitize clean

all: release

release: $(BUILD_DIR)/9top

$(BUILD_DIR)/9top: $(SOURCES) include/nine_top_metrics.h include/nine_top_ui.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@
	strip -x $@

test: $(BUILD_DIR)/test-ui $(BUILD_DIR)/9top
	$(BUILD_DIR)/test-ui
	test "$$($(BUILD_DIR)/9top --version)" = "9top 1.1.1"
	$(BUILD_DIR)/9top --help | grep -q '^Usage: 9top'
	! $(BUILD_DIR)/9top --interval inf >/dev/null 2>&1
	! $(BUILD_DIR)/9top --unknown >/dev/null 2>&1
	$(CC) $(CPPFLAGS) -std=c11 --analyze -Xanalyzer -analyzer-output=text $(SOURCES)

sanitize:
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fno-omit-frame-pointer -fsanitize=address,undefined -arch arm64 -mmacosx-version-min=14.0 tests/test_ui.c src/ui.c -o $(BUILD_DIR)/test-ui-sanitize
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 $(BUILD_DIR)/test-ui-sanitize
	$(CC) $(CPPFLAGS) -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fno-omit-frame-pointer -fsanitize=address,undefined -arch arm64 -mmacosx-version-min=14.0 $(SOURCES) $(LDLIBS) -o $(BUILD_DIR)/9top-sanitize

$(BUILD_DIR)/test-ui: tests/test_ui.c src/ui.c include/nine_top_ui.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_ui.c src/ui.c -o $@

clean:
	rm -rf $(BUILD_DIR)
