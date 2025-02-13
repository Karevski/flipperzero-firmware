#include "check.h"
#include "common_defines.h"

#include <furi_hal_console.h>
#include <furi_hal_power.h>
#include <furi_hal_rtc.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <stdlib.h>

PLACE_IN_SECTION("MB_MEM2") const char* __furi_check_message = NULL;
PLACE_IN_SECTION("MB_MEM2") uint32_t __furi_check_registers[12] = {0};

/** Load r12 value to __furi_check_message and store registers to __furi_check_registers */
#define GET_MESSAGE_AND_STORE_REGISTERS()               \
    asm volatile("ldr r11, =__furi_check_message    \n" \
                 "str r12, [r11]                    \n" \
                 "ldr r12, =__furi_check_registers  \n" \
                 "stm r12, {r0-r11}                 \n" \
                 :                                      \
                 :                                      \
                 : "memory");

#ifdef FURI_DEBUG
// Restore registers and halt MCU with bkpt mcu debug state
#define RESTORE_REGISTERS_AND_HALT_MCU()                \
    asm volatile("ldr r12, =__furi_check_registers  \n" \
                 "ldm r12, {r0-r11}                 \n" \
                 "loop%=:                           \n" \
                 "bkpt 0x00                         \n" \
                 "wfi                               \n" \
                 "b loop%=                          \n" \
                 :                                      \
                 :                                      \
                 : "memory");
#else
// Restore registers and halt MCU for release builds without bkpt instruction
#define RESTORE_REGISTERS_AND_HALT_MCU()                \
    asm volatile("ldr r12, =__furi_check_registers  \n" \
                 "ldm r12, {r0-r11}                 \n" \
                 "loop%=:                           \n" \
                 "wfi                               \n" \
                 "b loop%=                          \n" \
                 :                                      \
                 :                                      \
                 : "memory");
#endif

extern size_t xPortGetTotalHeapSize(void);
extern size_t xPortGetFreeHeapSize(void);
extern size_t xPortGetMinimumEverFreeHeapSize(void);

static void __furi_put_uint32_as_text(uint32_t data) {
    char tmp_str[] = "-2147483648";
    itoa(data, tmp_str, 10);
    furi_hal_console_puts(tmp_str);
}

static void __furi_print_stack_info() {
    furi_hal_console_puts("\r\n\tstack watermark: ");
    __furi_put_uint32_as_text(uxTaskGetStackHighWaterMark(NULL) * 4);
}

static void __furi_print_heap_info() {
    furi_hal_console_puts("\r\n\t     heap total: ");
    __furi_put_uint32_as_text(xPortGetTotalHeapSize());
    furi_hal_console_puts("\r\n\t      heap free: ");
    __furi_put_uint32_as_text(xPortGetFreeHeapSize());
    furi_hal_console_puts("\r\n\t heap watermark: ");
    __furi_put_uint32_as_text(xPortGetMinimumEverFreeHeapSize());
}

static void __furi_print_name(bool isr) {
    if(isr) {
        furi_hal_console_puts("[ISR ");
        __furi_put_uint32_as_text(__get_IPSR());
        furi_hal_console_puts("] ");
    } else {
        const char* name = pcTaskGetName(NULL);
        if(name == NULL) {
            furi_hal_console_puts("[main] ");
        } else {
            furi_hal_console_puts("[");
            furi_hal_console_puts(name);
            furi_hal_console_puts("] ");
        }
    }
}

FURI_NORETURN void __furi_crash() {
    __disable_irq();
    GET_MESSAGE_AND_STORE_REGISTERS();

    bool isr = FURI_IS_IRQ_MODE();

    if(__furi_check_message == NULL) {
        __furi_check_message = "Fatal Error";
    }

    furi_hal_console_puts("\r\n\033[0;31m[CRASH]");
    __furi_print_name(isr);
    furi_hal_console_puts(__furi_check_message);

    if(!isr) {
        __furi_print_stack_info();
    }
    __furi_print_heap_info();

#ifdef FURI_DEBUG
    furi_hal_console_puts("\r\nSystem halted. Connect debugger for more info\r\n");
    furi_hal_console_puts("\033[0m\r\n");
    RESTORE_REGISTERS_AND_HALT_MCU();
#else
    furi_hal_rtc_set_fault_data((uint32_t)__furi_check_message);
    furi_hal_console_puts("\r\nRebooting system.\r\n");
    furi_hal_console_puts("\033[0m\r\n");
    furi_hal_power_reset();
#endif
    __builtin_unreachable();
}

FURI_NORETURN void __furi_halt() {
    __disable_irq();
    GET_MESSAGE_AND_STORE_REGISTERS();

    bool isr = FURI_IS_IRQ_MODE();

    if(__furi_check_message == NULL) {
        __furi_check_message = "System halt requested.";
    }

    furi_hal_console_puts("\r\n\033[0;31m[HALT]");
    __furi_print_name(isr);
    furi_hal_console_puts(__furi_check_message);
    furi_hal_console_puts("\r\nSystem halted. Bye-bye!\r\n");
    furi_hal_console_puts("\033[0m\r\n");
    RESTORE_REGISTERS_AND_HALT_MCU();
    __builtin_unreachable();
}
