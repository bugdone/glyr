#ifndef DISCOGS_H
#define DISCOGS_H

#include "../types.h"

const char * cover_discogs_url(glyr_settings_t * sets);
memCache_t * cover_discogs_parse(cb_object *capo);

#endif
