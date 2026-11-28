#include "fake_bmw.h"
#include "gauge_core.h"
#include "diagnostic_worker.h"
#include "warm_units.h"
#include "surface_protocol.h"
#include <assert.h>
namespace bmw { class IDispatcher {}; class Bundle {}; }
static uint64_t clock_ms=10000;
static uint64_t test_clock() { return clock_ms; }
static bool no_units() { return false; }
static pid_t no_worker(const char* const*,int) { assert(false);return -1; }
static surface_state captured;
#define ID6_SURFACE_SESSION_H
class SurfaceSession {
public:
    void reap() {} void closeChannel() {} bool open(const char*) { return true; }
    bool sendState(const surface_state& value,uint64_t) { captured=value;return true; }
};
#define ID6_EMULATOR 1
#define ID6_PLUGIN_NAME "HostFaultEmulator"
#define ID6_ENTRY_PAGE 3
#define bd_mono_ms test_clock
#define launch no_worker
#define loadWarmFahrenheit no_units
#include "library.cpp"
#undef loadWarmFahrenheit
#undef launch
#undef bd_mono_ms
static void action(unsigned id) { bmw::Parcel event;assert(fake_fire(0,id,event)); }
static void poll_app(ID6LibraryLab& app) { bmw::Parcel event;assert(app.invokeDataUpdate(poll_key,event)); }
int main() {
    char directory[]="/tmp/full-fault-emulator.XXXXXX";assert(mkdtemp(directory));
    char path[128];snprintf(path,sizeof path,"%s/codes",directory);
    bmw::IDispatcher dispatcher;bmw::Bundle bundle;bmw::RHMIInterfaceVersion version={1};fake_reset();
    ID6LibraryLab app(dispatcher,bmw::BasicString<char>("HostFaultEmulator"),1,0,version,bundle,bundle,
                      path,path,path,path,path,path,path);
    assert(app.onInit() && access(path,F_OK)!=0 && !app.codesPrefs_.raw);
    app.focusPage(codes,true);poll_app(app);
    assert(app.codesComplete_ && strstr(captured.status,"synthetic") && captured.launch_mode==0);
    const std::string original=app.codeBuffer_;
    FullFaultPreferences prefs;prefs.raw=true;assert(prefs.save(path));
    poll_app(app);
    assert(app.codesFile_ && app.codesPrefs_.raw && captured.boost_style==1 && original==app.codeBuffer_);
    const unsigned services[]={3,7,10};const unsigned actions[]={5893,5897,5900};
    for (unsigned i=0;i<3;++i) {
        app.focusPage(codesSettings,true);action(actions[i]);app.focusPage(codes,true);poll_app(app);
        assert(captured.launch_mode==services[i] && strstr(captured.status,"synthetic"));
        assert(!strncmp(captured.values[0],"OBD",3));
    }
    const unsigned char invalid[]={'E',1,2,0};
    assert(saveFullScreenRecord(path,invalid,sizeof invalid));
    FullFaultPreferences damaged;assert(!damaged.load(path) && damaged.raw);
    FILE* file=fopen(path,"rb");assert(file);unsigned char unchanged[4];
    assert(fread(unchanged,1,sizeof unchanged,file)==4 && fclose(file)==0 && !memcmp(unchanged,invalid,4));
    damaged.raw=false;assert(!damaged.load(directory) && damaged.raw);
    app.onFinalize();assert(unlink(path)==0 && rmdir(directory)==0);
    puts("Full-screen fault emulator: private E1 first appearance, no diagnostic launches, distinct synthetic categories and fail-closed non-mutating preference loads passed.");
}
