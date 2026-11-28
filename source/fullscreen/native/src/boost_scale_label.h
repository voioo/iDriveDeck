#ifndef ID6_FULL_BOOST_SCALE_LABEL_H
#define ID6_FULL_BOOST_SCALE_LABEL_H
#include "gauge_core.h"
#include <stdio.h>

/* Surface metadata names display ranges, never measured pressure or ECU limits. */
static inline int full_boost_rail_caption(char* label,size_t capacity,unsigned maximum,unsigned units) {
    if ((maximum!=1000 && maximum!=1600 && maximum!=2000 && maximum!=2500) || units>2) return 0;
    const int length=snprintf(label,capacity,"Rail 0-%.0f",bd_display_selected(BD_RAIL,maximum,units));
    return length>0 && (size_t)length<capacity;
}
#endif
