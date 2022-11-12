#pragma once

#include <hf/bitops.h>

#define SPI_MAX_ID      (1024)
#define SPI_TABLE_SIZE  (SPI_MAX_ID >> 5)
#define VALID_SPI(nr)   ((nr) < SPI_MAX_ID)

struct spi_table {
    uint32_t map[SPI_TABLE_SIZE];
};

void spi_table_init(struct spi_table *table);

bool spi_is_allocated(struct spi_table *table, uint32_t spi);

bool allocate_spi(struct spi_table *table, uint32_t spi);

bool spi_requests(struct spi_table *src, struct spi_table *dist);
