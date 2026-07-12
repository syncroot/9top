CC := clang
CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -arch arm64 -mmacosx-version-min=14.0
LDLIBS := -framework IOKit -framework CoreFoundation
BUILD_DIR := build
SOURCES := src/main.c src/metrics.c src/ui.c

.PHONY: all release test clean

all: release

release: $(BUILD_DIR)/9top

$(BUILD_DIR)/9top: $(SOURCES) include/nine_top_metrics.h include/nine_top_ui.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDLIBS) -o $@
	strip -x $@

test: $(BUILD_DIR)/test-ui
	$(BUILD_DIR)/test-ui

$(BUILD_DIR)/test-ui: tests/test_ui.c src/ui.c include/nine_top_ui.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_ui.c src/ui.c -o $@

clean:
	rm -rf $(BUILD_DIR)
