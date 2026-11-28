#include "fake_bmw.h"
#include "gauge_core.h"
#include "diagnostic_worker.h"
#include "warm_units.h"
#include "surface_protocol.h"
#include <assert.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>

namespace bmw { class IDispatcher {}; class Bundle {}; }
static uint64_t clock_ms=10000;
static uint64_t test_clock() { return clock_ms; }
static bool no_units() { return false; }
static unsigned launches;
static unsigned cleanup_delay;
static unsigned response_delay;
static std::string fixture;
static std::vector<std::string> arguments;
static pid_t fixture_worker(const char* const* args,int output) {
    ++launches;arguments.clear();
    for (unsigned i=0;args[i];++i) arguments.push_back(args[i]);
    pid_t child=fork();assert(child>=0);
    if (!child) {
        signal(SIGTERM,SIG_IGN);
        ::poll(0,0,response_delay);
        size_t offset=0;
        while (offset<fixture.size()) {
            const ssize_t count=write(output,fixture.data()+offset,fixture.size()-offset);
            if (count<=0) _exit(3);
            offset+=static_cast<size_t>(count);
        }
        close(output);
        ::poll(0,0,cleanup_delay);
        _exit(0);
    }
    return child;
}
static surface_state captured;
#define ID6_SURFACE_SESSION_H
class SurfaceSession {
public:
    void reap() {} void closeChannel() {} bool open(const char*) { return true; }
    bool sendState(const surface_state& value,uint64_t) { captured=value;return true; }
};
#define ID6_PLUGIN_NAME "HostFaults"
#define ID6_ENTRY_PAGE 3
#define bd_mono_ms test_clock
#define launch fixture_worker
#define loadWarmFahrenheit no_units
#include "library.cpp"
#undef loadWarmFahrenheit
#undef launch
#undef bd_mono_ms

static void action(unsigned id) { bmw::Parcel event;assert(fake_fire(0,id,event)); }
static void poll_app(ID6LibraryLab& app) { bmw::Parcel event;assert(app.invokeDataUpdate(poll_key,event)); }
static void result(ID6LibraryLab& app) {
    for (unsigned i=0;i<2000 && !app.codesComplete_;++i) { poll_app(app);::poll(0,0,1); }
    assert(app.codesComplete_ && app.socket_<0);
}
static void retire(ID6LibraryLab& app) {
    for (unsigned i=0;i<2000 && app.child_>0;++i) { app.reap();::poll(0,0,1); }
    assert(app.child_<0);
}
static bool arg(const char* value) {
    for (size_t i=0;i<arguments.size();++i) if (arguments[i]==value) return true;
    return false;
}
static std::string model(unsigned id) {
    for (size_t i=fake_updates.size();i;--i) if (fake_updates[i-1].model==id) return fake_updates[i-1].text;
    assert(false);return "";
}
static void capture(const char* name) {
    const char* directory=getenv("FULL_FAULT_CAPTURE_DIR");if (!directory) return;
    char path[512];const int size=snprintf(path,sizeof path,"%s/%s.bin",directory,name);
    assert(size>0 && static_cast<unsigned>(size)<sizeof path);
    FILE* file=fopen(path,"wb");assert(file);
    assert(fwrite(&captured,1,sizeof captured,file)==sizeof captured && fclose(file)==0);
}

int main() {
    char directory[]="/tmp/full-fault-plugin.XXXXXX";assert(mkdtemp(directory));
    char preference[128],missing[128];
    snprintf(preference,sizeof preference,"%s/codes",directory);
    snprintf(missing,sizeof missing,"%s/no-directory/codes",directory);
    bmw::IDispatcher dispatcher;bmw::Bundle bundle;bmw::RHMIInterfaceVersion version={1};
    fake_reset();
    ID6LibraryLab app(dispatcher,bmw::BasicString<char>("HostFaults"),1,0,version,bundle,bundle,
                      missing,missing,missing,missing,missing,missing,preference);
    assert(app.onInit() && !app.codesPrefs_.raw && access(preference,F_OK)!=0);
    fixture="BMW: 3 codes\n000001  status 09 mask 08\n120308  status 04 mask FF\nFFFFFF  status 80 mask 00\n";
    cleanup_delay=200;
    app.focusPage(codes,true);result(app);
    assert(arg("--codes") && arg("--status-mask") && !arg("--clear") && launches==1);
    assert(model(1460)=="000001  Confirmed" && model(1461)=="120308  Pending" && model(1462)=="FFFFFF  Unknown");
    assert(!strcmp(captured.values[0],"000001  status 09 mask 08") && captured.launch_mode==0);
    capture("gasoline");
    app.focusPage(codesSettings,true);action(5881);app.focusPage(codes,true);poll_app(app);
    assert(app.codesPrefs_.raw && captured.boost_style==1 && launches==1);
    assert(model(1870)=="Status 09 / supported 08");capture("raw");
    const std::string old_row=captured.values[0];
    app.codesPreferences_=missing;
    app.focusPage(codesSettings,true);action(5880);app.focusPage(codes,true);poll_app(app);
    assert(app.codesPrefs_.raw && !app.codesSaved_ && launches==1 && old_row==captured.values[0]);
    assert(strstr(captured.status,"Settings not saved"));
    app.codesPreferences_=preference;
    FullFaultPreferences restarted;assert(restarted.load(preference) && restarted.raw);

    // EOF does not imply that the previous worker has finished bounded cleanup.
    assert(app.child_>0);const pid_t old=app.child_;
    fixture="OBD Confirmed emissions: 1 code\nOBD03 P0133\n";
    app.focusPage(codesSettings,true);action(5893);app.focusPage(codes,true);poll_app(app);
    assert(launches==1 && app.child_==old && captured.values[0][0]==0 && app.codesResult_==0);
    assert(model(1460).empty() && model(1870).empty());
    result(app);
    assert(launches==2 && arg("--obd-confirmed") && !arg("--codes") && !arg("--status-mask") && !arg("--clear"));
    assert(captured.launch_mode==3 && model(1460)=="P0133  Confirmed");capture("confirmed");
    cleanup_delay=0;retire(app);
    const unsigned services[]={7,10};const unsigned actions[]={5897,5900};
    const char* flags[]={"--obd-pending","--obd-permanent"};
    const char* records[]={"OBD07 C1234\n","OBD0A U1016\n"};
    for (unsigned i=0;i<2;++i) {
        fixture=std::string("OBD synthetic category\n")+records[i];
        app.focusPage(codesSettings,true);action(actions[i]);app.focusPage(codes,true);result(app);
        assert(captured.launch_mode==services[i] && arg(flags[i]) && !arg("--clear") && !arg("--status-mask"));
        capture(i ? "permanent" : "pending");retire(app);
    }
    const unsigned before=launches;
    action(234);poll_app(app);assert(launches==before && !app.codesClearPending_);
    app.focusPage(codesConfirm,true);app.focusPage(codes,true);poll_app(app);
    assert(launches==before); // cancel does not issue any command
    app.focusPage(codesConfirm,true);action(234);
    clock_ms+=5001;app.focusPage(codes,true);poll_app(app);
    assert(launches==before && strstr(captured.status,"expired") && !app.codesClearPending_);
    fixture="BMW confirmed-clear fixture\n000001  status 00 mask FF\n";
    app.focusPage(codesConfirm,true);action(234);app.focusPage(codes,true);result(app);
    assert(launches==before+1 && arg("--clear") && arg("--confirm-clear") && arg("--status-mask"));
    assert(captured.launch_mode==0 && model(1460)=="000001  Unknown");retire(app);

    // Navigating to display settings cancels a pending read without auto-repeating it.
    response_delay=150;fixture="OBD Pending emissions: 1 code\nOBD07 P0101\n";
    app.focusPage(codesSettings,true);action(5897);app.focusPage(codes,true);poll_app(app);
    const unsigned cancelled=launches;assert(app.socket_>=0 && !app.codesComplete_);
    app.focusPage(codesSettings,true);action(5881);app.focusPage(codes,true);poll_app(app);
    assert(launches==cancelled && !app.codesComplete_ && captured.values[0][0]==0);
    retire(app);poll_app(app);assert(launches==cancelled);
    response_delay=0;

    // All 128 BMW records fit, including the availability mask and the last page.
    fixture="BMW large fixture\n";
    for (unsigned i=0;i<128;++i) { char line[40];snprintf(line,sizeof line,"%06X  status 09 mask 08\n",i+1);fixture+=line; }
    action(5890);result(app);assert(!app.codesIncomplete_);
    for (unsigned i=0;i<31;++i) action(230);
    assert(model(1463)=="000080  Confirmed");action(230);assert(model(1460)=="000001  Confirmed");retire(app);
    fixture="BMW incomplete fixture\n000001  status 09";
    action(5890);result(app);
    assert(app.codesIncomplete_ && model(1460)=="Incomplete diagnostic output" && strstr(captured.status,"Incomplete output"));
    retire(app);
    fixture=std::string("BMW overflow fixture\n")+std::string(5000,'X');
    action(5890);result(app);assert(app.codesIncomplete_ && app.codeSize_<sizeof app.codeBuffer_);retire(app);
    fixture=std::string("BMW invalid text\nABC",20)+std::string(1,'\0')+"000001  status 09 mask FF\n";
    action(5890);result(app);assert(app.codesIncomplete_);retire(app);
    app.onFinalize();
    assert(unlink(preference)==0 && rmdir(directory)==0);
    puts("Full-screen faults: production callbacks, masked status, raw persistence/save failure, read categories, worker retirement, bounded pipe/paging and confirmed-clear expiry passed.");
}
