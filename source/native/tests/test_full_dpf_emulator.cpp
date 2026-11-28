#include "fake_bmw.h"
#include "gauge_core.h"
#include "diagnostic_worker.h"
#include "warm_units.h"
#include "surface_protocol.h"
#include "emulator_telemetry.h"
#include <assert.h>
namespace bmw { class IDispatcher {}; class Bundle {}; }
static uint64_t clock_ms=10000;
static uint64_t test_clock() { return clock_ms; }
static bool no_units() { return false; }
static pid_t no_worker(const char* const*,int) { assert(false);return -1; }
static char telemetry_path[128];
static surface_state captured;
#define ID6_SURFACE_SESSION_H
class SurfaceSession {
public:
    void reap() {} void closeChannel() {} bool open(const char*) { return true; }
    bool sendState(const surface_state& value,uint64_t) { captured=value;return true; }
};
#undef IDEMU_TELEMETRY_FILE
#define IDEMU_TELEMETRY_FILE telemetry_path
#define ID6_EMULATOR 1
#define ID6_PLUGIN_NAME "HostDpfEmulator"
#define ID6_ENTRY_PAGE 8
#define bd_mono_ms test_clock
#define launch no_worker
#define loadWarmFahrenheit no_units
#include "library.cpp"
#undef loadWarmFahrenheit
#undef launch
#undef bd_mono_ms
static void poll_app(ID6LibraryLab& app) { bmw::Parcel event;assert(app.invokeDataUpdate(poll_key,event)); }
static void sample() {
    emulator_telemetry value={clock_ms,1500,0,75,0,0,0,69};
    FILE* file=fopen(telemetry_path,"wb");assert(file);
    assert(fwrite(&value,1,sizeof value,file)==sizeof value && fclose(file)==0);
}
int main() {
    char directory[]="/tmp/full-dpf-emulator.XXXXXX";assert(mkdtemp(directory));
    char path[128];snprintf(path,sizeof path,"%s/dpf",directory);
    snprintf(telemetry_path,sizeof telemetry_path,"%s/feed",directory);
    bmw::IDispatcher dispatcher;bmw::Bundle bundle;bmw::RHMIInterfaceVersion version={1};fake_reset();
    ID6LibraryLab app(dispatcher,bmw::BasicString<char>("HostDpfEmulator"),1,0,version,bundle,bundle,
        path,path,path,path,path,path,path,path);
    assert(app.onInit() && access(path,F_OK)!=0 && !app.dpfPrefs_.regen);
    sample();app.focusPage(dpf,true);poll_app(app);
    assert(strstr(captured.status,"synthetic") && !strcmp(captured.values[0],"25.0 mbar"));
    assert(!strcmp(captured.values[6],"Regeneration: Not shown"));
    FullDpfPreferences prefs;prefs.bank=1;prefs.metric=false;prefs.regen=true;assert(prefs.save(path));
    clock_ms+=20;sample();poll_app(app);
    assert(app.dpfFile_ && app.dpfPrefs_.bank==1 && !app.dpfPrefs_.metric && app.dpfPrefs_.regen);
    assert(!strcmp(captured.values[0],"0.218 psi") && !strcmp(captured.values[5],"167.0 \260F"));
    assert(!strcmp(captured.values[6],"Regeneration: Passive") && strstr(captured.status,"synthetic"));
    clock_ms+=600;poll_app(app);assert(!strcmp(captured.values[0],"Unknown"));
    app.onFinalize();assert(unlink(path)==0 && unlink(telemetry_path)==0 && rmdir(directory)==0);
    puts("Full-screen DPF emulator: labeled synthetic feed, no diagnostic worker launch, private D2 appearance, units/bank/status and stale-feed clearing passed.");
}
