#include <hf/mm.h>
#include <hf/io.h>
#include <hf/std.h>
#include "ns16550.h"

#define NS16550_IO32(x) IO32_C((uintptr_t)x)

#ifdef NS16550_BASE_ADDRESS
#undef NS16550_BASE_ADDRESS
#define NS16550_BASE_ADDRESS    (0x3100000)
#endif

static NS16550_t g_ns16550 = NULL;

void NS16550_init(uintptr_t base_addr)
{
    /* TODO: interrupt configuration */
    g_ns16550 = (NS16550_t)base_addr;
}

void NS16550_putc(NS16550_t port, char c)
{
    if ( c == '\n' ) {
        NS16550_putc(port, '\r');
    }

    while ((io_read32(NS16550_IO32(&port->lsr)) & UART_LSR_THRE) == 0)
        ;

    io_write32(NS16550_IO32(&port->thr), (uint32_t)c);
}

void plat_console_init(void)
{
    NS16550_init((uintptr_t)0x3100000);
}

NS16550_t get_tegra_ns16550(void)
{
    return g_ns16550;
}

void plat_console_putchar(char c)
{
    NS16550_t port = get_tegra_ns16550();

    NS16550_putc(port, c);
}

void plat_console_mm_init(
    struct mm_stage1_locked stage1_locked, struct mpool *ppool)
{
    /* Map page for UART. */
    (void)stage1_locked;
    (void)ppool;
    mm_identity_map(stage1_locked, pa_init(0x3100000),
        pa_add(pa_init(0x3100000), PAGE_SIZE),
        MM_MODE_R | MM_MODE_W | MM_MODE_D, ppool);
}
