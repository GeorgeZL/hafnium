#include <hf/interrupt.h>

static inline void spi_table_init(struct spi_table *table)
{
    if (table) {
        memset(table, 0, sizeof(struct spi_table));
    }
}

static inline bool spi_is_allocated(struct spi_table *table, uint32_t spi)
{
    if (table && VALID_SPI(spi) ) {
        return test_bit(spi, table->map);
    }

    return false;
}

static inline bool allocate_spi(struct spi_table *table, uint32_t spi)
{
    bool retval = false;

    if (VALID_SPI(spi) && !irq_is_allocated(table, spi)) {
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
