#ifndef L_LYRDB_H
#define L_LYRDB_H

#include "../types.h"

const char * lyrics_lyrdb_url(glyr_settings_t * settings);
memCache_t * lyrics_lyrdb_parse(cb_object *capo);

#endif
