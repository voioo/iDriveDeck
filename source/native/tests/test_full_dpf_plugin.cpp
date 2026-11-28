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
static volatile sig_atomic_t retiring;
static void retire_signal(int) { retiring=1; }
static unsigned launches;
static std::vector<std::string> arguments;
static pid_t fake_worker(const char* const* args,int) {
    ++launches;arguments.clear();for (unsigned i=0;args[i];++i) arguments.push_back(args[i]);
    int ready[2];assert(pipe(ready)==0);const pid_t parent=getpid();
    const pid_t child=fork();assert(child>=0);
    if (!child) {
        close(ready[0]);signal(SIGTERM,retire_signal);retiring=0;
        const char value='r';if (write(ready[1],&value,1)!=1) _exit(3);close(ready[1]);
        while (!retiring && getppid()==parent) ::poll(0,0,10);
        ::poll(0,0,200);_exit(0);
    }
    close(ready[1]);char value;assert(read(ready[0],&value,1)==1);close(ready[0]);return child;
}
static surface_state captured;
#define ID6_SURFACE_SESSION_H
class SurfaceSession {
public:
    void reap() {} void closeChannel() {} bool open(const char*) { return true; }
    bool sendState(const surface_state& value,uint64_t) { captured=value;return true; }
};
#define ID6_PLUGIN_NAME "HostDpf"
#define ID6_ENTRY_PAGE 8
#define bd_mono_ms test_clock
#define launch fake_worker
#define loadWarmFahrenheit no_units
#include "library.cpp"
#undef loadWarmFahrenheit
#undef launch
#undef bd_mono_ms

static void action(unsigned id) { bmw::Parcel event;assert(fake_fire(0,id,event)); }
static void poll_app(ID6LibraryLab& app) { bmw::Parcel event;assert(app.invokeDataUpdate(poll_key,event)); }
static bool arg(const char* value) {
    for (size_t i=0;i<arguments.size();++i) if (arguments[i]==value) return true;
    return false;
}
static std::string model(unsigned id) {
    for (size_t i=fake_updates.size();i;--i) if (fake_updates[i-1].model==id) return fake_updates[i-1].text;
    assert(false);return "";
}
static void send(ID6LibraryLab& app,int sender,const dpf_frame& frame,bool poll=true) {
    char packet[DPF_MAX_PACKET];const int size=dpf_encode(&frame,packet,sizeof packet);assert(size>0);
    const sockaddr_in peer=gauge_local_address(port(dpf));
    assert(sendto(sender,packet,size,0,reinterpret_cast<const sockaddr*>(&peer),sizeof peer)==size);
    if (poll) poll_app(app);
}
static void retire(ID6LibraryLab& app) {
    for (unsigned i=0;i<2000 && app.child_>0;++i) { app.reap();::poll(0,0,1); }
    assert(app.child_<0);
}
static void capture(const char* name) {
    const char* directory=getenv("FULL_DPF_CAPTURE_DIR");if (!directory) return;
    char path[512];const int size=snprintf(path,sizeof path,"%s/%s.bin",directory,name);
    assert(size>0 && static_cast<unsigned>(size)<sizeof path);
    FILE* file=fopen(path,"wb");assert(file);
    assert(fwrite(&captured,1,sizeof captured,file)==sizeof captured && fclose(file)==0);
}
int main() {
    char directory[]="/tmp/full-dpf-plugin.XXXXXX";assert(mkdtemp(directory));
    char preference[128],missing[128];snprintf(preference,sizeof preference,"%s/dpf",directory);
    snprintf(missing,sizeof missing,"%s/missing/file",directory);
    bmw::IDispatcher dispatcher;bmw::Bundle bundle;bmw::RHMIInterfaceVersion version={1};fake_reset();
    ID6LibraryLab app(dispatcher,bmw::BasicString<char>("HostDpf"),1,0,version,bundle,bundle,
        missing,missing,missing,missing,missing,missing,missing,preference);
    assert(app.onInit() && !app.dpfPrefs_.regen && app.dpfPrefs_.metric && access(preference,F_OK)!=0);
    app.focusPage(dpf,true);poll_app(app);
    assert(launches==1 && arg("--token") && arg("--udp-port") && !arg("--regen-status"));
    assert(strstr(arguments[0].c_str(),"dpfdeck-collector-qnx") && app.child_>0);
    assert(model(1560)=="Unknown" && model(1566)=="Regeneration: Not shown");
    const pid_t child=app.child_;
    int sender=gauge_bind(DPF_ACTIVE_PORT);assert(sender>=0);
    int wrong=socket(AF_INET,SOCK_DGRAM,0);assert(wrong>=0);
    dpf_frame frame;memset(&frame,0,sizeof frame);frame.worker_pid=static_cast<uint32_t>(child);
    frame.token=app.dpfToken_;frame.sent_ms=clock_ms;frame.seq=1;frame.known=DPF_ALL;
    frame.supported=DPF_ALL & ~DPF_BIT(DPF_B1_OUTLET_PRESSURE);
    frame.valid=frame.supported & ~DPF_BIT(DPF_B1_INLET_TEMP);
    frame.value[DPF_B1_INLET_PRESSURE]=100;frame.value[DPF_B1_OUTLET_TEMP]=420;
    frame.value[DPF_B2_DELTA]=10;frame.value[DPF_B2_INLET_PRESSURE]=110;frame.value[DPF_B2_OUTLET_PRESSURE]=100;
    frame.value[DPF_B2_INLET_TEMP]=390;frame.value[DPF_B2_OUTLET_TEMP]=400;frame.value[DPF_COOLANT]=75;
    frame.value[DPF_REGEN_PROGRESS]=1;frame.value[DPF_REGEN_TYPE]=1;
    send(app,wrong,frame);assert(model(1560)=="Unknown");
    --frame.token;send(app,sender,frame);assert(model(1560)=="Unknown");++frame.token;
    ++frame.worker_pid;send(app,sender,frame);assert(model(1560)=="Unknown");--frame.worker_pid;
    ++frame.sent_ms;send(app,sender,frame);assert(model(1560)=="Unknown");--frame.sent_ms;
    send(app,sender,frame);
    assert(model(1560)=="0.0 mbar" && model(1561)=="1000.0 mbar" && model(1562)=="Unsupported");
    assert(model(1563)=="Unavailable" && model(1566)=="Regeneration: Not shown");
    assert(captured.boost_signal[0]==DPF_VALID && captured.boost_signal[2]==DPF_UNSUPPORTED);
    capture("metric-partial");
    action(5921);action(5923);
    assert(app.child_==child && !app.childStopping_ && launches==1);
    assert(model(1560)=="1.450 psi" && model(1561)=="15.954 psi" && model(1563)=="734.0 \260F");
    assert(model(1567)=="Bank 2 | psi, F");capture("imperial-live");
    FullDpfPreferences persisted;assert(persisted.load(preference) && persisted.bank==1 && !persisted.metric && !persisted.regen);
    app.dpfPreferences_=missing;action(5920);
    assert(app.dpfPrefs_.bank==1 && !app.dpfSaved_ && strstr(captured.status,"Could not save"));
    assert(model(1923)=="Could not save DPF settings" && app.child_==child);
    app.dpfPreferences_=preference;action(5922);

    // Duplicate/old sequence is ignored, and each poll drains at most eight packets.
    frame.value[DPF_COOLANT]=99;send(app,sender,frame);assert(model(1565)=="75.0 \260C");
    for (unsigned i=0;i<9;++i) { frame.seq=2+i;frame.value[DPF_COOLANT]=80+i;send(app,sender,frame,false); }
    poll_app(app);assert(model(1565)=="87.0 \260C");poll_app(app);assert(model(1565)=="88.0 \260C");
    const uint64_t old_token=app.dpfToken_;
    action(5925);poll_app(app);
    assert(app.child_==child && app.childStopping_ && launches==1 && app.socket_<0);
    retire(app);poll_app(app);
    assert(launches==2 && arg("--regen-status") && app.dpfToken_!=old_token);
    frame.worker_pid=static_cast<uint32_t>(app.child_);frame.token=app.dpfToken_;frame.seq=1;
    frame.value[DPF_REGEN_PROGRESS]=1;frame.value[DPF_REGEN_TYPE]=1;
    send(app,sender,frame);assert(model(1566)=="Regeneration: Active");capture("regen-active");
    ++frame.seq;frame.value[DPF_REGEN_TYPE]=0;send(app,sender,frame);assert(model(1566)=="Regeneration: Passive");
    ++frame.seq;frame.value[DPF_REGEN_PROGRESS]=0;send(app,sender,frame);assert(model(1566)=="Regeneration: Not in progress");
    clock_ms+=DPF_STALE_MS;poll_app(app);
    assert(model(1560)=="Stale" && model(1566)=="Regeneration: Stale");capture("stale");
    app.onFinalize();retire(app);assert(app.socket_<0 && captured.page==surface_hidden);
    close(sender);close(wrong);assert(unlink(preference)==0 && rmdir(directory)==0);
    puts("Full-screen DPF callbacks/UDP: private settings, source/token/PID/sequence/time guards, bounded drain, bank/units, optional status restart and retired-worker/exit cleanup passed.");
}
