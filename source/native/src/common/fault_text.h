#ifndef ID6_FAULT_TEXT_H
#define ID6_FAULT_TEXT_H
#include "fault_codes.h"
#include "obd_fault_codes.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Shared worker-text contract for the native and full-screen consumers. */
typedef struct FaultLine {
    uint32_t code;
    unsigned status, available;
    bool has_mask;
} FaultLine;

static inline int fault_text_hex(const char* data,unsigned length,unsigned* out) {
    if (!data || !out) return 0;
    unsigned value=0;
    for (unsigned i=0;i<length;++i) {
        unsigned digit;
        if (data[i]>='0' && data[i]<='9') digit=(unsigned)(data[i]-'0');
        else if (data[i]>='A' && data[i]<='F') digit=(unsigned)(data[i]-'A'+10);
        else if (data[i]>='a' && data[i]<='f') digit=(unsigned)(data[i]-'a'+10);
        else return 0;
        value=(value<<4)|digit;
    }
    *out=value;return 1;
}

static inline int fault_text_parse(const char* line,size_t length,FaultLine* out) {
    if (!line || !out || (length!=17 && length!=25) || memcmp(line+6,"  status ",9)) return 0;
    unsigned code,status,available=0;
    if (!fault_text_hex(line,6,&code) || !fault_text_hex(line+15,2,&status)) return 0;
    if (length==25 && (memcmp(line+17," mask ",6) || !fault_text_hex(line+23,2,&available))) return 0;
    out->code=code;out->status=status;out->available=available;out->has_mask=length==25;
    return 1;
}

static inline int fault_text_obd(const char* line,size_t length,unsigned service,char code[6]) {
    if (!obd_fault_service(service) || !line || !code || length!=11 || memcmp(line,"OBD",3) || line[5]!=' ') return 0;
    static const char families[]="PCBU";
    const char* family=strchr(families,line[6]);
    unsigned received,digits;
    if (!fault_text_hex(line+3,2,&received) || received!=service || !family || !line[6] ||
        line[7]<'0' || line[7]>'3' || !fault_text_hex(line+8,3,&digits)) return 0;
    const uint16_t value=(uint16_t)(((unsigned)(family-families)<<14)|((unsigned)(line[7]-'0')<<12)|digits);
    if (!value) return 0;
    obd_fault_format(value,code);return 1;
}

static inline const char* fault_text_state(const FaultLine* line) {
    if (!line || !line->has_mask) return "Raw";
    const unsigned flags=line->status & line->available;
    if (flags & FAULT_TEST_FAILED) return "Active";
    if (flags & FAULT_PENDING) return "Pending";
    if (flags & FAULT_CONFIRMED) return "Confirmed";
    if (flags & FAULT_FAILED_THIS_CYCLE) return "This cycle";
    if (flags & FAULT_FAILED_SINCE_CLEAR) return "Since clear";
    if (flags & FAULT_WARNING_REQUESTED) return "Warning";
    return "Unknown";
}
#endif
