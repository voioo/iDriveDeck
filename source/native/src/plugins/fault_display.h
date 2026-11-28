#ifndef FAULT_DISPLAY_H
#define FAULT_DISPLAY_H
#include "fault_text.h"

inline bool faultHex(const char* data,unsigned length,unsigned& out) {
    return fault_text_hex(data,length,&out)!=0;
}
inline bool parseFaultLine(const char* line,size_t length,FaultLine& out) {
    return fault_text_parse(line,length,&out)!=0;
}
inline bool parseObdFaultLine(const char* line,size_t length,unsigned service,char code[6]) {
    return fault_text_obd(line,length,service,code)!=0;
}
inline const char* faultState(const FaultLine& line) { return fault_text_state(&line); }
#endif
