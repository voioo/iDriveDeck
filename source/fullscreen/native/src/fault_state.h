#ifndef ID6_FULL_FAULT_STATE_H
#define ID6_FULL_FAULT_STATE_H
#include "../../../native/src/plugins/fault_preferences.h"
#include "fullscreen_preferences.h"
#include "fault_files.h"

struct FullFaultPreferences : FaultPreferences {
    bool save(const char* path) const {
        if (!path || !*path) return false;
        const unsigned char data[]={'E',1,static_cast<unsigned char>(raw),0};
        return saveFullScreenRecord(path,data,sizeof data);
    }
};
#endif
