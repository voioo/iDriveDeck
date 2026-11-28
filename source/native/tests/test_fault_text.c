#include "fault_text.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    FaultLine line;
    assert(fault_text_parse("100213  status 09 mask 08",25,&line));
    assert(line.code==0x100213 && line.status==9 && line.available==8 && line.has_mask);
    assert(!strcmp(fault_text_state(&line),"Confirmed"));
    assert(fault_text_parse("000001  status 09",17,&line));
    assert(line.code==1 && !line.has_mask && !strcmp(fault_text_state(&line),"Raw"));
    static const unsigned bits[]={1,4,8,2,32,128};
    static const char* const names[]={"Active","Pending","Confirmed","This cycle","Since clear","Warning"};
    for (unsigned status=0;status<256;++status) for (unsigned mask=0;mask<256;++mask) {
        line.status=status;line.available=mask;line.has_mask=true;
        const char* expected="Unknown";
        for (unsigned i=0;i<6;++i) if ((status&mask)&bits[i]) { expected=names[i];break; }
        assert(!strcmp(fault_text_state(&line),expected));
    }
    const char* invalid[]={"+00001  status 09 mask 08","0x0001  status 09 mask 08",
        "100213  status 0G mask 08","100213  status 09 mask GG","100213  status 09 mask 08x"};
    for (unsigned i=0;i<sizeof invalid/sizeof invalid[0];++i)
        assert(!fault_text_parse(invalid[i],strlen(invalid[i]),&line));
    assert(!fault_text_parse(0,25,&line) && !fault_text_parse("",0,&line));
    char code[6];
    assert(fault_text_obd("OBD03 P0123",11,3,code) && !strcmp(code,"P0123"));
    assert(fault_text_obd("OBD07 C1234",11,7,code) && !strcmp(code,"C1234"));
    assert(fault_text_obd("OBD0A U3001",11,10,code) && !strcmp(code,"U3001"));
    assert(!fault_text_obd("OBD03 P0123",11,7,code));
    assert(!fault_text_obd("OBD03 P0000",11,3,code));
    assert(!fault_text_obd("OBD03 P4123",11,3,code));
    assert(!fault_text_obd("OBD03 P0123x",12,3,code));
    puts("Fault text C: exact records, all status/mask pairs, legacy unknown support and distinct emissions categories passed.");
}
