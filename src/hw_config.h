#ifndef HW_CONFIG_H
#define HW_CONFIG_H

#include "sd_card.h"
#include <stddef.h>

size_t sd_get_num();
sd_card_t *sd_get_by_num(size_t num);

#endif
