#include "nine_top_metrics.h"
#include "nine_top_ui.h"

#include <errno.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define ESC "\033["
#define COLOR_RESET ESC "0m"
#define COLOR_TITLE ESC "30;46;1m"
#define COLOR_CYAN ESC "96;1m"
#define COLOR_GREEN ESC "92m"
#define COLOR_YELLOW ESC "93;1m"
#define COLOR_RED ESC "91;1m"
#define COLOR_MUTED ESC "90m"
#define COLOR_FOOTER ESC "30;42m"

typedef struct {
    nine_top_metrics metrics;
    double load[3];
} snapshot;

static struct termios original_terminal;
static int terminal_active;
static int colors_enabled;

static const char *color_for(nine_top_level level) {
    switch (level) {
        case NINE_TOP_NORMAL: return COLOR_GREEN;
        case NINE_TOP_ELEVATED: return COLOR_YELLOW;
        case NINE_TOP_HIGH: return COLOR_RED;
        default: return COLOR_MUTED;
    }
}

static double monotonic_seconds(void) {
    struct timespec value = {0};
    clock_gettime(CLOCK_MONOTONIC, &value);
    return (double)value.tv_sec + (double)value.tv_nsec / 1e9;
}

static void restore_terminal(void) {
    if (!terminal_active) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_terminal);
    fputs(ESC "?25h" ESC "?1049l", stdout);
    fflush(stdout);
    terminal_active = 0;
}

static int enter_terminal(void) {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) return -1;
    if (tcgetattr(STDIN_FILENO, &original_terminal) != 0) return -1;
    struct termios raw = original_terminal;
    cfmakeraw(&raw);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return -1;
    terminal_active = 1;
    atexit(restore_terminal);
    fputs(ESC "?1049h" ESC "?25l" ESC "2J", stdout);
    fflush(stdout);
    return 0;
}

static void terminal_size(size_t *width, size_t *height) {
    struct winsize value = {0};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &value) == 0 && value.ws_col && value.ws_row) {
        *width = value.ws_col;
        *height = value.ws_row;
    } else {
        *width = 80;
        *height = 24;
    }
}

static void format_number(char *output, size_t size, double value, int decimals) {
    if (isfinite(value)) snprintf(output, size, "%.*f", decimals, value);
    else snprintf(output, size, "--");
}

static void format_gib(char *output, size_t size, uint64_t bytes) {
    snprintf(output, size, "%.1f", (double)bytes / 1073741824.0);
}

static void draw_line(size_t row, size_t height, size_t width, const char *text, const char *color) {
    char *fitted = malloc(width + 1);
    if (!fitted) return;
    nine_top_fit(fitted, width + 1, text, width);
    if (colors_enabled && color && *color) fputs(color, stdout);
    fputs(fitted, stdout);
    if (colors_enabled && color && *color) fputs(COLOR_RESET, stdout);
    if (row + 1 < height) fputs("\r\n", stdout);
    free(fitted);
}

static void meter(char *output, size_t size, size_t width, const char *label,
                  double value, double maximum, const char *suffix) {
    size_t bar_width = width > 32 ? width - 28 : 4;
    if (bar_width > 512) bar_width = 512;
    double ratio = isfinite(value) ? fmin(1, fmax(0, value / maximum)) : 0;
    size_t filled = (size_t)llround((double)bar_width * ratio);
    char bar[513];
    memset(bar, '|', filled);
    memset(bar + filled, '.', bar_width - filled);
    bar[bar_width] = '\0';
    char shown[32];
    format_number(shown, sizeof(shown), value, 1);
    snprintf(output, size, "  %-11s [%s] %6s %s", label, bar, shown, suffix);
}

static snapshot read_snapshot(int include_battery, double previous_battery) {
    snapshot result;
    memset(&result, 0, sizeof(result));
    nine_top_read_metrics(&result.metrics, include_battery);
    if (!include_battery) result.metrics.battery_temperature = previous_battery;
    if (getloadavg(result.load, 3) < 0) memset(result.load, 0, sizeof(result.load));
    return result;
}

static void render(const snapshot *value, size_t width, size_t height, int paused) {
    fputs(ESC "H", stdout);
    if (width < 20 || height < 5) {
        fputs(ESC "2J" ESC "H", stdout);
        draw_line(0, height, width, "9top: terminal too small", COLOR_YELLOW);
        fflush(stdout);
        return;
    }

    size_t row = 0;
    char line[2048];
    char cpu_temp[32], gpu_temp[32], battery_temp[32], cpu_use[32], gpu_use[32];
    format_number(cpu_temp, sizeof(cpu_temp), value->metrics.cpu_temperature, 1);
    format_number(gpu_temp, sizeof(gpu_temp), value->metrics.gpu_temperature, 1);
    format_number(battery_temp, sizeof(battery_temp), value->metrics.battery_temperature, 1);
    format_number(cpu_use, sizeof(cpu_use), value->metrics.cpu_usage, 0);
    format_number(gpu_use, sizeof(gpu_use), value->metrics.gpu_usage, 0);

    time_t now = time(NULL);
    struct tm local = {0};
    localtime_r(&now, &local);
    char clock[32];
    strftime(clock, sizeof(clock), "%H:%M:%S", &local);
    snprintf(line, sizeof(line), " 9top  %s  %s", paused ? "PAUSED" : "LIVE", clock);
    draw_line(row++, height, width, line, COLOR_TITLE);

    if (width < 64 || height < 16) {
        double cpu_worst = fmax(isfinite(value->metrics.cpu_temperature) ? value->metrics.cpu_temperature : 0,
                                isfinite(value->metrics.cpu_usage) ? value->metrics.cpu_usage : 0);
        snprintf(line, sizeof(line), " CPU  %s C   load %s%%", cpu_temp, cpu_use);
        draw_line(row++, height, width, line, color_for(nine_top_classify(cpu_worst, 70, 85)));
        double gpu_worst = fmax(isfinite(value->metrics.gpu_temperature) ? value->metrics.gpu_temperature : 0,
                                isfinite(value->metrics.gpu_usage) ? value->metrics.gpu_usage : 0);
        snprintf(line, sizeof(line), " GPU  %s C   load %s%%", gpu_temp, gpu_use);
        draw_line(row++, height, width, line, color_for(nine_top_classify(gpu_worst, 70, 85)));
        snprintf(line, sizeof(line), " BAT  %s C", battery_temp);
        draw_line(row++, height, width, line,
                  color_for(nine_top_classify(value->metrics.battery_temperature, 35, 40)));
        if (row < height - 1) {
            double memory = nine_top_percent(value->metrics.memory_used, value->metrics.memory_total);
            char used[32], total[32], memory_text[32];
            format_gib(used, sizeof(used), value->metrics.memory_used);
            format_gib(total, sizeof(total), value->metrics.memory_total);
            format_number(memory_text, sizeof(memory_text), memory, 0);
            snprintf(line, sizeof(line), " MEM  %s%%  %s/%s GiB", memory_text, used, total);
            draw_line(row++, height, width, line, color_for(nine_top_classify(memory, 75, 90)));
        }
        if (row < height - 1) {
            snprintf(line, sizeof(line), " LOAD %.2f %.2f %.2f", value->load[0], value->load[1], value->load[2]);
            draw_line(row++, height, width, line, NULL);
        }
    } else {
        snprintf(line, sizeof(line), " THERMALS %.*s", (int)fmin(1500, width - 11),
                 "--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------");
        draw_line(row++, height, width, line, COLOR_CYAN);
        meter(line, sizeof(line), width, "CPU average", value->metrics.cpu_temperature, 105, "C");
        draw_line(row++, height, width, line, color_for(nine_top_classify(value->metrics.cpu_temperature, 70, 85)));
        meter(line, sizeof(line), width, "GPU average", value->metrics.gpu_temperature, 105, "C");
        draw_line(row++, height, width, line, color_for(nine_top_classify(value->metrics.gpu_temperature, 70, 85)));
        meter(line, sizeof(line), width, "Battery", value->metrics.battery_temperature, 50, "C");
        draw_line(row++, height, width, line, color_for(nine_top_classify(value->metrics.battery_temperature, 35, 40)));
        draw_line(row++, height, width, "", NULL);
        snprintf(line, sizeof(line), " SYSTEM %.*s", (int)fmin(1500, width - 9),
                 "--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------");
        draw_line(row++, height, width, line, COLOR_CYAN);
        meter(line, sizeof(line), width, "CPU load", value->metrics.cpu_usage, 100, "%");
        draw_line(row++, height, width, line, color_for(nine_top_classify(value->metrics.cpu_usage, 70, 90)));
        meter(line, sizeof(line), width, "GPU load", value->metrics.gpu_usage, 100, "%");
        draw_line(row++, height, width, line, color_for(nine_top_classify(value->metrics.gpu_usage, 70, 90)));
        double memory = nine_top_percent(value->metrics.memory_used, value->metrics.memory_total);
        meter(line, sizeof(line), width, "Memory", memory, 100, "%");
        draw_line(row++, height, width, line, color_for(nine_top_classify(memory, 75, 90)));
        char used[32], total[32];
        format_gib(used, sizeof(used), value->metrics.memory_used);
        format_gib(total, sizeof(total), value->metrics.memory_total);
        snprintf(line, sizeof(line), "  Memory      %s / %s GiB", used, total);
        draw_line(row++, height, width, line, COLOR_MUTED);
        double swap = nine_top_percent(value->metrics.swap_used, value->metrics.swap_total);
        meter(line, sizeof(line), width, "Swap", swap, 100, "%");
        draw_line(row++, height, width, line, color_for(nine_top_classify(swap, 25, 60)));
        snprintf(line, sizeof(line), "  Load avg    %.2f  %.2f  %.2f", value->load[0], value->load[1], value->load[2]);
        draw_line(row++, height, width, line, NULL);
    }

    while (row < height - 1) draw_line(row++, height, width, "", NULL);
    draw_line(row, height, width, " q quit   p pause", COLOR_FOOTER);
    fflush(stdout);
}

static double parse_interval(int argc, char **argv) {
    for (int index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--version") == 0) {
            puts("9top 1.1.0");
            exit(0);
        }
        if ((strcmp(argv[index], "--interval") == 0 || strcmp(argv[index], "-i") == 0)
            && index + 1 < argc) {
            char *end = NULL;
            double value = strtod(argv[index + 1], &end);
            if (end && *end == '\0' && value >= 1) return value;
            fprintf(stderr, "9top: interval must be at least one second\n");
            exit(2);
        }
    }
    return 2;
}

int main(int argc, char **argv) {
    double interval = parse_interval(argc, argv);
    if (enter_terminal() != 0) {
        fprintf(stderr, "9top: an interactive terminal is required\n");
        return 1;
    }
    nine_top_install_signal_handlers();
    const char *term = getenv("TERM");
    colors_enabled = getenv("NO_COLOR") == NULL && (!term || strcmp(term, "dumb") != 0);

    snapshot current = read_snapshot(1, NAN);
    double last_sample = monotonic_seconds();
    double last_battery = last_sample;
    size_t previous_width = 0, previous_height = 0;
    int paused = 0, dirty = 1;

    while (!nine_top_should_terminate()) {
        size_t width, height;
        terminal_size(&width, &height);
        if (width != previous_width || height != previous_height) {
            previous_width = width;
            previous_height = height;
            dirty = 1;
        }
        double now = monotonic_seconds();
        if (!paused && now - last_sample >= interval) {
            int include_battery = now - last_battery >= 30;
            current = read_snapshot(include_battery, current.metrics.battery_temperature);
            last_sample = now;
            if (include_battery) last_battery = now;
            dirty = 1;
        }
        if (dirty) {
            render(&current, width, height, paused);
            dirty = 0;
        }

        struct pollfd input = { .fd = STDIN_FILENO, .events = POLLIN };
        if (poll(&input, 1, 200) > 0 && (input.revents & POLLIN)) {
            unsigned char key = 0;
            if (read(STDIN_FILENO, &key, 1) == 1) {
                if (key == 'q' || key == 'Q' || key == 3) break;
                if (key == 'p' || key == 'P' || key == ' ') {
                    paused = !paused;
                    dirty = 1;
                }
            }
        }
    }
    nine_top_close_metrics();
    restore_terminal();
    return 0;
}
