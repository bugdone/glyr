#ifndef AMAZON_H
#define AMAZON_H

#include "../core.h"

const char * cover_amazon_url(glyr_settings_t * sets);
cache_list * cover_amazon_parse(cb_object *capo);

#endif
