#ifndef ID6_FULL_FAULT_VIEW_H
#define ID6_FULL_FAULT_VIEW_H
#include "../../../native/src/common/fault_text.h"
#include "dme_descriptions.h"
#include <stdio.h>

static inline int full_fault_service_valid(unsigned service) {
    return !service || obd_fault_service(service);
}

/* Both display paths use the same strict parser; OBD codes never enter the BMW lookup. */
static inline void full_fault_row(const char* row,unsigned service,unsigned raw,
                                 char* title,size_t title_size,char* detail,size_t detail_size) {
    title[0]=detail[0]=0;
    if (!row || !*row) return;
    if (!full_fault_service_valid(service) || raw>1) {
        snprintf(title,title_size,"Diagnostic result unavailable");return;
    }
    const size_t length=strlen(row);
    if (!service) {
        FaultLine line;
        if (fault_text_parse(row,length,&line)) {
            snprintf(title,title_size,"%06X  %s",(unsigned)line.code,fault_text_state(&line));
            if (!raw) snprintf(detail,detail_size,"%s",dme_code_description(line.code));
            else if (line.has_mask)
                snprintf(detail,detail_size,"Status %02X / supported %02X",line.status,line.available);
            else snprintf(detail,detail_size,"Status %02X / mask unavailable",line.status);
            return;
        }
    } else {
        char code[6];
        if (fault_text_obd(row,length,service,code)) {
            snprintf(title,title_size,"%s  %s",code,obd_fault_state(service));
            if (service==10) snprintf(detail,detail_size,"Permanent emissions code; removal is ECU-controlled");
            else snprintf(detail,detail_size,"Standard OBD-II %s emissions code",obd_fault_state(service));
            return;
        }
    }
    /* Diagnostics can also return plain refusal, truncation or transport notices. */
    snprintf(title,title_size,"%s",row);
}
#endif
