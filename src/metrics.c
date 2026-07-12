#include "nine_top_metrics.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>

typedef struct {
    uint8_t major, minor, build, reserved;
    uint16_t release;
} smc_version;

typedef struct {
    uint16_t version, length;
    uint32_t cpu_limit, gpu_limit, memory_limit;
} smc_limits;

typedef struct {
    uint32_t size, type;
    uint8_t attributes;
} smc_key_info;

typedef struct {
    uint32_t key;
    smc_version version;
    smc_limits limits;
    smc_key_info info;
    uint8_t result, status, command;
    uint32_t data32;
    uint8_t bytes[32];
} smc_key_data;

enum { SMC_SELECTOR = 2, SMC_READ_BYTES = 5, SMC_READ_INDEX = 8, SMC_READ_INFO = 9 };
static const uint32_t SMC_FLOAT = 0x666c7420; // "flt "

static io_connect_t smc_connection;
static char cpu_keys[128][5];
static char gpu_keys[128][5];
static size_t cpu_key_count, gpu_key_count;
static int smc_initialized;
static host_cpu_load_info_data_t previous_cpu;
static int has_previous_cpu;
static volatile sig_atomic_t termination_requested;

static void request_termination(int signal_number) {
    (void)signal_number;
    termination_requested = 1;
}

void nine_top_install_signal_handlers(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = request_termination;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
}

int nine_top_should_terminate(void) {
    return termination_requested != 0;
}

static uint32_t fourcc(const char key[4]) {
    return ((uint32_t)(uint8_t)key[0] << 24) | ((uint32_t)(uint8_t)key[1] << 16)
         | ((uint32_t)(uint8_t)key[2] << 8) | (uint32_t)(uint8_t)key[3];
}

static int smc_call(const smc_key_data *input, smc_key_data *output) {
    size_t output_size = sizeof(*output);
    memset(output, 0, sizeof(*output));
    kern_return_t result = IOConnectCallStructMethod(
        smc_connection, SMC_SELECTOR, input, sizeof(*input), output, &output_size);
    return result == KERN_SUCCESS && output->result == 0 ? 0 : -1;
}

static int smc_key_info_for(uint32_t key, smc_key_info *info) {
    smc_key_data input = {0}, output = {0};
    input.key = key;
    input.command = SMC_READ_INFO;
    if (smc_call(&input, &output) != 0) return -1;
    *info = output.info;
    return 0;
}

static int smc_read_float(const char name[4], double *value) {
    smc_key_data input = {0}, output = {0};
    input.key = fourcc(name);
    if (smc_key_info_for(input.key, &input.info) != 0) return -1;
    if (input.info.size != 4 || input.info.type != SMC_FLOAT) return -1;
    input.command = SMC_READ_BYTES;
    if (smc_call(&input, &output) != 0) return -1;
    float decoded;
    memcpy(&decoded, output.bytes, sizeof(decoded));
    if (!isfinite(decoded) || decoded <= 0 || decoded > 150) return -1;
    *value = decoded;
    return 0;
}

static int smc_open(void) {
    io_iterator_t iterator = IO_OBJECT_NULL;
    CFMutableDictionaryRef matching = IOServiceMatching("AppleSMC");
    if (!matching || IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) != KERN_SUCCESS)
        return -1;

    io_service_t service;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        io_name_t name = {0};
        IORegistryEntryGetName(service, name);
        if (strcmp(name, "AppleSMCKeysEndpoint") == 0) {
            IOServiceOpen(service, mach_task_self(), 0, &smc_connection);
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);
    return smc_connection == IO_OBJECT_NULL ? -1 : 0;
}

static void smc_initialize(void) {
    if (smc_initialized) return;
    smc_initialized = 1;
    if (smc_open() != 0) return;

    smc_key_info count_info;
    smc_key_data input = {0}, output = {0};
    input.key = fourcc("#KEY");
    if (smc_key_info_for(input.key, &count_info) != 0) return;
    input.info = count_info;
    input.command = SMC_READ_BYTES;
    if (smc_call(&input, &output) != 0) return;
    uint32_t raw_count = 0;
    memcpy(&raw_count, output.bytes, sizeof(raw_count));
    uint32_t count = __builtin_bswap32(raw_count);
    if (count > 16384) return;

    for (uint32_t index = 0; index < count; index++) {
        memset(&input, 0, sizeof(input));
        input.command = SMC_READ_INDEX;
        input.data32 = index;
        if (smc_call(&input, &output) != 0) continue;
        char key[5] = {
            (char)(output.key >> 24), (char)(output.key >> 16),
            (char)(output.key >> 8), (char)output.key, 0
        };
        double probe;
        if (smc_read_float(key, &probe) != 0) continue;
        int is_cpu = key[0] == 'T' && (key[1] == 'p' || key[1] == 'e' || key[1] == 's');
        int is_gpu = key[0] == 'T' && key[1] == 'g';
        if (is_cpu && cpu_key_count < 128) memcpy(cpu_keys[cpu_key_count++], key, 5);
        if (is_gpu && gpu_key_count < 128) memcpy(gpu_keys[gpu_key_count++], key, 5);
    }
}

static double average_temperature(char keys[][5], size_t count) {
    double total = 0;
    size_t valid = 0;
    for (size_t index = 0; index < count; index++) {
        double value;
        if (smc_read_float(keys[index], &value) == 0) {
            total += value;
            valid++;
        }
    }
    return valid ? total / (double)valid : NAN;
}

static double battery_temperature(void) {
    io_service_t battery = IOServiceGetMatchingService(
        kIOMainPortDefault, IOServiceMatching("AppleSmartBattery"));
    if (battery == IO_OBJECT_NULL) return NAN;
    CFTypeRef property = IORegistryEntryCreateCFProperty(
        battery, CFSTR("Temperature"), kCFAllocatorDefault, 0);
    IOObjectRelease(battery);
    if (!property || CFGetTypeID(property) != CFNumberGetTypeID()) {
        if (property) CFRelease(property);
        return NAN;
    }
    int raw = 0;
    CFNumberGetValue((CFNumberRef)property, kCFNumberIntType, &raw);
    CFRelease(property);
    double result = raw / 10.0 - 273.15;
    return result >= -20 && result <= 100 ? result : NAN;
}

static double gpu_usage(void) {
    io_iterator_t iterator = IO_OBJECT_NULL;
    CFMutableDictionaryRef matching = IOServiceMatching("IOAccelerator");
    if (!matching || IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) != KERN_SUCCESS)
        return NAN;

    double result = NAN;
    io_service_t service;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        CFTypeRef property = IORegistryEntryCreateCFProperty(
            service, CFSTR("PerformanceStatistics"), kCFAllocatorDefault, 0);
        if (property && CFGetTypeID(property) == CFDictionaryGetTypeID()) {
            CFTypeRef value = CFDictionaryGetValue(
                (CFDictionaryRef)property, CFSTR("Device Utilization %"));
            if (value && CFGetTypeID(value) == CFNumberGetTypeID()) {
                double candidate = 0;
                if (CFNumberGetValue((CFNumberRef)value, kCFNumberDoubleType, &candidate)
                    && candidate >= 0 && candidate <= 100) result = candidate;
            }
        }
        if (property) CFRelease(property);
        IOObjectRelease(service);
        if (isfinite(result)) break;
    }
    IOObjectRelease(iterator);
    return result;
}

static double cpu_usage(void) {
    host_cpu_load_info_data_t current = {0};
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    mach_port_t host = mach_host_self();
    kern_return_t result = host_statistics(host, HOST_CPU_LOAD_INFO,
                                           (host_info_t)&current, &count);
    mach_port_deallocate(mach_task_self(), host);
    if (result != KERN_SUCCESS) return NAN;
    if (!has_previous_cpu) {
        previous_cpu = current;
        has_previous_cpu = 1;
        return NAN;
    }
    uint64_t busy = 0, idle = 0;
    for (int i = 0; i < CPU_STATE_MAX; i++) {
        uint32_t delta = current.cpu_ticks[i] - previous_cpu.cpu_ticks[i];
        if (i == CPU_STATE_IDLE) idle += delta; else busy += delta;
    }
    previous_cpu = current;
    return busy + idle ? (double)busy / (double)(busy + idle) * 100.0 : 0;
}

static int memory_metrics(nine_top_metrics *metrics) {
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    size_t size = sizeof(metrics->memory_total);
    if (sysctl(mib, 2, &metrics->memory_total, &size, NULL, 0) != 0) return -1;

    vm_statistics64_data_t vm = {0};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    mach_port_t host = mach_host_self();
    kern_return_t result = host_statistics64(host, HOST_VM_INFO64,
                                             (host_info64_t)&vm, &count);
    mach_port_deallocate(mach_task_self(), host);
    if (result != KERN_SUCCESS) return -1;
    int64_t pages = (int64_t)vm.active_count + vm.inactive_count + vm.wire_count
                  + vm.speculative_count + vm.compressor_page_count
                  - vm.purgeable_count - vm.external_page_count;
    metrics->memory_used = (uint64_t)(pages > 0 ? pages : 0) * (uint64_t)vm_kernel_page_size;

    struct xsw_usage swap = {0};
    size = sizeof(swap);
    if (sysctlbyname("vm.swapusage", &swap, &size, NULL, 0) == 0) {
        metrics->swap_used = swap.xsu_used;
        metrics->swap_total = swap.xsu_total;
    }
    return 0;
}

int nine_top_read_metrics(nine_top_metrics *metrics, int include_battery) {
    if (!metrics) return -1;
    memset(metrics, 0, sizeof(*metrics));
    smc_initialize();
    metrics->cpu_temperature = average_temperature(cpu_keys, cpu_key_count);
    metrics->gpu_temperature = average_temperature(gpu_keys, gpu_key_count);
    metrics->battery_temperature = include_battery ? battery_temperature() : NAN;
    metrics->cpu_usage = cpu_usage();
    metrics->gpu_usage = gpu_usage();
    return memory_metrics(metrics);
}

void nine_top_close_metrics(void) {
    if (smc_connection != IO_OBJECT_NULL) {
        IOServiceClose(smc_connection);
        smc_connection = IO_OBJECT_NULL;
    }
}
