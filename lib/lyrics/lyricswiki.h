#ifndef L_LYRICSWIKI_H
#define L_LYRICSWIKI_H

#include "../core.h"

const char * lyrics_lyricswiki_url(glyr_settings_t * settings);
cache_list * lyrics_lyricswiki_parse(cb_object *capo);

#endif
