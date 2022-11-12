#include <hf/arch/arch.h>
#include <hf/io.h>
#include <hf/static_assert.h>
#include <hf/std.h>
#include <hf/errno.h>
#include <hf/dlog.h>
#include <hf/mm.h>
#include <hf/spinlock.h>
#include <hf/bitops.h>
#include <hf/interrupt.h>

void *memset(void *s, int c, size_t n);

void spi_table_init(struct spi_table *table)
{
    if (table) {
        memset(table, 0, sizeof(struct spi_table));
    }
}

bool spi_is_allocated(struct spi_table *table, uint32_t spi)
{
    if (table && VALID_SPI(spi) ) {
        return test_bit(spi, table->map);
    }

    return false;
}

bool allocate_spi(struct spi_table *table, uint32_t spi)
{
    bool retval = false;

    if (VALID_SPI(spi) && !spi_is_allocated(table, spi)) {
        set_bit(spi, table->map);
        retval = true;
    }

    return retval;
}

bool spi_requests(struct spi_table *src, struct spi_table *dist)
{
    uint32_t index = 0;
    uint32_t conflict = 0;
    bool success = true;

    for (index = 0; index < SPI_TABLE_SIZE; index++) {
        conflict = src->map[index] & dist->map[index];
        if (conflict) {
            dlog_error("SPI-IRQ: configuration conflict\n");
            success = false;
            break;
        }

        dist->map[index] |= src->map[index];
    }

    return success;
}
