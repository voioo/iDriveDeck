#ifndef ID6_FULL_DPF_STATE_H
#define ID6_FULL_DPF_STATE_H
#include "../../../native/src/common/dpf_core.h"
#include "fullscreen_preferences.h"
#include "dpf_files.h"
#include <string.h>
#include <sys/types.h>

struct FullDpfPreferences {
    unsigned bank;
    bool metric,regen;
    FullDpfPreferences() : bank(0),metric(true),regen(false) {}
    bool load(const char* path) {
        *this=FullDpfPreferences();
        FILE* file=fopen(path,"rb");if (!file) return false;
        unsigned char bytes[5];const size_t size=fread(bytes,1,sizeof bytes,file);
        bool complete=!ferror(file);
        if (complete && !feof(file)) complete=fgetc(file)==EOF && !ferror(file);
        fclose(file);
        if (!complete || size<4 || bytes[0]!='D' || bytes[2]>1 || bytes[3]>1 ||
            !((size==4 && bytes[1]==1) || (size==5 && bytes[1]==2 && bytes[4]<=1))) return false;
        bank=bytes[2];metric=bytes[3]!=0;regen=size==5 && bytes[4]!=0;return true;
    }
    bool save(const char* path) const {
        if (!path || !*path || bank>1) return false;
        const unsigned char bytes[]={'D',2,static_cast<unsigned char>(bank),
            static_cast<unsigned char>(metric),static_cast<unsigned char>(regen)};
        return saveFullScreenRecord(path,bytes,sizeof bytes);
    }
};

class FullDpfState {
    dpf_frame frame_;
    uint64_t opened_;
public:
    FullDpfState() { reset(0); }
    void reset(uint64_t now) { memset(&frame_,0,sizeof frame_);opened_=now; }
    bool accept(const char* data,size_t size,uint64_t now,uint64_t token,pid_t worker) {
        dpf_frame next;
        if (worker<=0 || !token || !dpf_parse(data,size,&next) || next.token!=token ||
            next.worker_pid!=static_cast<uint32_t>(worker) || !next.sent_ms || next.sent_ms<opened_ ||
            next.sent_ms>now || now-next.sent_ms>=DPF_STALE_MS) return false;
        if (frame_.sent_ms) {
            const uint32_t advance=next.seq-frame_.seq;
            if (!advance || advance>UINT32_C(0x7fffffff) || next.sent_ms<frame_.sent_ms) return false;
        }
        frame_=next;return true;
    }
    static unsigned field(unsigned slot,unsigned bank) {
        static const unsigned fields[2][6]={
            {DPF_B1_DELTA,DPF_B1_INLET_PRESSURE,DPF_B1_OUTLET_PRESSURE,DPF_B1_INLET_TEMP,DPF_B1_OUTLET_TEMP,DPF_COOLANT},
            {DPF_B2_DELTA,DPF_B2_INLET_PRESSURE,DPF_B2_OUTLET_PRESSURE,DPF_B2_INLET_TEMP,DPF_B2_OUTLET_TEMP,DPF_COOLANT}};
        return bank<2 && slot<6 ? fields[bank][slot] : static_cast<unsigned>(DPF_COUNT);
    }
    int state(unsigned field,uint64_t now) const {
        const uint64_t elapsed=frame_.sent_ms && frame_.sent_ms<=now ? now-frame_.sent_ms : static_cast<uint64_t>(DPF_STALE_MS);
        int value=dpf_measurement_state(&frame_,field,elapsed);
        if (value==DPF_VALID && (frame_.age_ms[field]>frame_.sent_ms ||
            frame_.sent_ms-frame_.age_ms[field]<opened_)) value=DPF_UNAVAILABLE;
        return value;
    }
    static const char* unavailable(int state) {
        return state==DPF_UNSUPPORTED ? "Unsupported" : state==DPF_UNAVAILABLE ? "Unavailable" :
               state==DPF_STALE ? "Stale" : "Unknown";
    }
    void reading(unsigned slot,const FullDpfPreferences& prefs,uint64_t now,char* out,size_t size) const {
        const unsigned id=field(slot,prefs.bank);const int current=state(id,now);
        if (current!=DPF_VALID) { snprintf(out,size,"%s",unavailable(current));return; }
        double value=dpf_display_value(id,frame_.value[id],prefs.metric);
        if (value==0) value=0; // avoid displaying negative zero after conversion
        snprintf(out,size,"%.*f %s%s",id<DPF_B1_INLET_TEMP && !prefs.metric ? 3 : 1,value,
                 id>=DPF_B1_INLET_TEMP ? "\260" : "",dpf_unit(id,prefs.metric));
    }
    const char* regeneration(bool enabled,uint64_t now) const {
        if (!enabled) return "Not shown";
        const int progress=state(DPF_REGEN_PROGRESS,now),type=state(DPF_REGEN_TYPE,now);
        return progress==DPF_VALID ? frame_.value[DPF_REGEN_PROGRESS]==0 ? "Not in progress" :
               type==DPF_VALID ? frame_.value[DPF_REGEN_TYPE]==1 ? "Active" : "Passive" : "In progress" :
               unavailable(progress);
    }
};
#endif
