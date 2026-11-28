#include "dpf_state.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void bytes(const char* path,const unsigned char* expected,size_t count) {
    unsigned char data[16];FILE* file=fopen(path,"rb");assert(file);
    assert(fread(data,1,sizeof data,file)==count && !ferror(file) && fclose(file)==0);
    assert(!memcmp(data,expected,count));
}
int main() {
    char directory[]="/tmp/full-dpf-state.XXXXXX";assert(mkdtemp(directory));
    char legacy[128],current[128];snprintf(legacy,sizeof legacy,"%s/legacy",directory);
    snprintf(current,sizeof current,"%s/full",directory);
    const unsigned char old[]={'D',1,1,0};assert(savePersistentBytes(legacy,old,sizeof old));
    FullDpfPreferences prefs;assert(prefs.load(legacy) && prefs.bank==1 && !prefs.metric && !prefs.regen);
    bytes(legacy,old,sizeof old);prefs.regen=true;assert(prefs.save(current));
    const unsigned char saved[]={'D',2,1,0,1};bytes(current,saved,sizeof saved);bytes(legacy,old,sizeof old);
    FullDpfPreferences restarted;assert(restarted.load(current) && restarted.regen && !restarted.metric && restarted.bank==1);
    const unsigned char bad[][6]={{'D',2,0,1,2,0},{'D',2,2,1,1,0},{'D',2,0,2,1,0},
        {'D',3,0,1,1,0},{'D',2,0,1,1,0},{'D',1,0,1,1,0},{'D',2,0,1,0,0}};
    const unsigned lengths[]={5,5,5,5,6,5,4};
    for (unsigned i=0;i<7;++i) {
        assert(savePersistentBytes(current,bad[i],lengths[i]));
        prefs.bank=1;prefs.metric=false;prefs.regen=true;
        assert(!prefs.load(current) && prefs.bank==0 && prefs.metric && !prefs.regen);
        bytes(current,bad[i],lengths[i]);
    }
    FullDpfState state;state.reset(1000);dpf_frame frame;memset(&frame,0,sizeof frame);
    frame.worker_pid=123;frame.token=42;frame.sent_ms=1000;frame.seq=UINT32_MAX;
    frame.known=DPF_ALL;frame.supported=DPF_ALL & ~DPF_BIT(DPF_B2_DELTA);
    frame.valid=frame.supported & ~DPF_BIT(DPF_B1_OUTLET_TEMP);
    frame.value[DPF_B1_DELTA]=0;frame.value[DPF_B1_INLET_TEMP]=410;frame.value[DPF_COOLANT]=75;
    frame.value[DPF_REGEN_PROGRESS]=1;frame.value[DPF_REGEN_TYPE]=0;
    char packet[DPF_MAX_PACKET],text[72];int length=dpf_encode(&frame,packet,sizeof packet);assert(length>0);
    assert(!state.accept(packet,length,1000,41,123) && !state.accept(packet,length,1000,42,124));
    assert(state.accept(packet,length,1000,42,123));
    prefs=FullDpfPreferences();state.reading(0,prefs,1000,text,sizeof text);assert(!strcmp(text,"0.0 mbar"));
    assert(state.state(DPF_B1_OUTLET_TEMP,1000)==DPF_UNAVAILABLE);
    assert(state.state(DPF_B2_DELTA,1000)==DPF_UNSUPPORTED);
    assert(!strcmp(state.regeneration(false,1000),"Not shown") && !strcmp(state.regeneration(true,1000),"Passive"));
    assert(!state.accept(packet,length,1000,42,123));
    frame.seq=1;frame.sent_ms=1100;frame.age_ms[DPF_COOLANT]=100;
    length=dpf_encode(&frame,packet,sizeof packet);assert(state.accept(packet,length,1100,42,123));
    assert(state.state(DPF_COOLANT,5499)==DPF_VALID && state.state(DPF_COOLANT,5500)==DPF_STALE);
    prefs.metric=false;state.reading(3,prefs,1100,text,sizeof text);assert(!strcmp(text,"770.0 \260F"));
    frame.seq=2;frame.sent_ms=1200;frame.age_ms[DPF_COOLANT]=201;
    length=dpf_encode(&frame,packet,sizeof packet);assert(state.accept(packet,length,1200,42,123));
    assert(state.state(DPF_COOLANT,1200)==DPF_UNAVAILABLE);
    frame.seq=3;frame.sent_ms=1300;frame.age_ms[DPF_COOLANT]=0;
    length=dpf_encode(&frame,packet,sizeof packet);assert(!state.accept(packet,length,1299,42,123));
    assert(!state.accept(packet,length-1,1300,42,123));
    assert(unlink(legacy)==0 && unlink(current)==0 && rmdir(directory)==0);
    puts("Full-screen DPF: D1/D2 files, migration without rewrites, fail-closed flags, source/generation/sequence/time, units, valid zero and independent stale/unavailable states passed.");
}
