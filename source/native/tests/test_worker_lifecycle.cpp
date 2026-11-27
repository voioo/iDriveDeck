// Run one production plugin per executable. No diagnostic worker is executed.
#include "fake_bmw.h"
#include "gauge_core.h"
#include "diagnostic_worker.h"
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

namespace bmw { class IDispatcher {}; class Bundle {}; }
static uint64_t now_ms=1000;
extern "C" uint64_t bd_mono_ms(void) { return now_ms; }
static pid_t tracked=700001, real_child=-1;
static int wait_result=-1, wait_error=ECHILD, probe_error=0;
static unsigned waits=0, probes=0, terms=0, launches=0;
static bool real_probes=false;

static void require(bool condition, const char* message) {
    if (!condition) { fprintf(stderr,"%s: %s\n",LIFECYCLE_SOURCE,message); exit(1); }
}
static pid_t lifecycle_waitpid(pid_t pid, int* status, int options) {
    require(pid==tracked && options==WNOHANG,"unexpected wait target/options");
    ++waits;
    if (status) *status=0;
    if (wait_result<0) errno=wait_error;
    return wait_result>0 ? pid : wait_result;
}
static int lifecycle_kill(pid_t pid, int signal) {
    require(pid==tracked && (signal==0 || signal==SIGTERM),"unexpected signal target/type");
    if (!signal) {
        ++probes;
        if (real_probes) return ::kill(pid,0);
        if (probe_error) { errno=probe_error; return -1; }
        return 0;
    }
    ++terms;
    if (real_probes) {
        require(pid==real_child && pid>0,"refusing to signal an unowned host process");
        return ::kill(pid,SIGTERM);
    }
    return 0;
}
static pid_t lifecycle_launch(const char* const* args, int) {
    require(args && args[0],"missing child argv");
    for (unsigned i=0;args[i];++i)
        require(std::string(args[i])!="--clear", "lifecycle tests must never request a clear");
    ++launches; tracked=700001+static_cast<pid_t>(launches);
    return tracked; // no spawn, exec, diagnostics or vehicle connection
}

// System headers above retain their real declarations. Only production calls
// below are intercepted, so the real-child cleanup code can use the host OS.
#define waitpid lifecycle_waitpid
#define kill lifecycle_kill
#define launch lifecycle_launch
#include LIFECYCLE_SOURCE
#undef launch
#undef kill
#undef waitpid

#if TEST_boost_deck
typedef BoostDeck Plugin;
static pid_t& owned_pid(Plugin& p,bool tone) { return tone ? p.tone_ : p.collector_; }
static void reap(Plugin& p,bool tone) { if (tone) p.reapTone(); else p.reapCollector(); }
static void stop(Plugin& p,bool tone) { if (tone) p.stopTone(); else p.stopCollector(); }
#elif TEST_shift_deck
typedef ShiftDeck Plugin;
static pid_t& owned_pid(Plugin& p,bool) { return p.child_; }
static void reap(Plugin& p,bool) { p.reap(); }
static void stop(Plugin& p,bool) { p.stopChild(); }
#elif TEST_warm_deck
typedef WarmDeck Plugin;
static pid_t& owned_pid(Plugin& p,bool) { return p.child_; }
static void reap(Plugin& p,bool) { p.reap(); }
static void stop(Plugin& p,bool) { p.stopWorker(); }
#elif TEST_engine_codes
typedef EngineCodes Plugin;
static pid_t& owned_pid(Plugin& p,bool) { return p.codes_pid_; }
static void reap(Plugin& p,bool) { p.reapWorker(); }
static void stop(Plugin& p,bool) { p.stopWorker(); }
#elif TEST_hud_menu
typedef HudTest Plugin;
static pid_t& owned_pid(Plugin& p,bool) { return p.child_; }
static void reap(Plugin& p,bool) { bmw::Parcel event; p.invokeDataUpdate(kPoll,event); }
static void stop(Plugin& p,bool) { p.enabled_=true; bmw::Parcel event; p.show(event); }
#elif TEST_beamer
typedef LightShow Plugin;
static pid_t& owned_pid(Plugin& p,bool) { return p.child_; }
static void reap(Plugin& p,bool) { p.reap(); }
static void stop(Plugin& p,bool) { p.stopWorker(); }
#elif TEST_bootmod3_suite
typedef Bootmod3SuiteApp Plugin;
static pid_t& owned_pid(Plugin& p,bool) { return p.child_; }
static void reap(Plugin& p,bool) { p.reap(); }
static void stop(Plugin& p,bool) { p.stopWorker(); }
#elif TEST_hybrid_deck
typedef HybridDeck Plugin;
static pid_t& owned_pid(Plugin& p,bool) { return p.child_; }
static void reap(Plugin& p,bool) { p.reap(); }
static void stop(Plugin& p,bool) { p.stopWorker(); }
#elif TEST_launch_deck
typedef LaunchDeck Plugin;
static pid_t& owned_pid(Plugin& p,bool) { return p.child_; }
static void reap(Plugin& p,bool) { p.reap(); }
static void stop(Plugin& p,bool) { p.stopChild(); }
#elif TEST_mg_flasher
typedef MgFlasher Plugin;
static pid_t& owned_pid(Plugin& p,bool) { return p.child_; }
static void reap(Plugin& p,bool) { p.reap(); }
static void stop(Plugin& p,bool) { p.shutdown(); }
#elif TEST_mhd_maps
typedef MhdMaps Plugin;
static pid_t& owned_pid(Plugin& p,bool) { return p.child_; }
static void reap(Plugin& p,bool) { p.reap(); }
static void stop(Plugin& p,bool) { p.stopWorker(); }
#elif TEST_road_jet
typedef RoadJet Plugin;
static pid_t& owned_pid(Plugin& p,bool) { return p.child_; }
static void reap(Plugin& p,bool) { p.reap(); }
static void stop(Plugin& p,bool) { p.shutdown(); }
#elif TEST_xdrive_control
typedef XDriveControlApp Plugin;
static pid_t& owned_pid(Plugin& p,bool) { return p.child_; }
static void reap(Plugin& p,bool) { p.reap(); }
static void stop(Plugin& p,bool) { p.stopWorker(); }
#else
#error Add a production adapter for this plugin
#endif

static void state_matrix(Plugin& p,bool tone) {
    struct Case { int result,error,probe; bool gone; };
    const Case cases[]={{0,0,0,false},{-1,EINTR,0,false},{-1,ECHILD,0,false},
        {-1,ECHILD,EPERM,false},{-1,ECHILD,EAGAIN,false},{-1,ECHILD,EINTR,false},
        {-1,ECHILD,ESRCH,true},{1,0,0,true}};
    for (unsigned i=0;i<sizeof cases/sizeof cases[0];++i) {
        tracked=700001; owned_pid(p,tone)=tracked;
        wait_result=cases[i].result; wait_error=cases[i].error; probe_error=cases[i].probe;
        waits=probes=terms=0;
        reap(p,tone);
        require(cases[i].gone ? owned_pid(p,tone)<=0 : owned_pid(p,tone)==tracked,
                "lost a live/uncertain worker or retained a confirmed dead worker");
        require(waits==1 && !terms,"reaping must use one nonblocking wait and no termination signal");
        require(probes==static_cast<unsigned>(wait_result<0 && wait_error==ECHILD),
                "process existence must be checked only after ECHILD");
    }
    owned_pid(p,tone)=-1; waits=probes=terms=0;
    reap(p,tone); require(!waits && !probes && !terms,"reaper touched an invalid PID");
}

static void real_process(Plugin& p,bool tone) {
    real_child=fork(); require(real_child>=0,"fork failed");
    if (!real_child) {
        const pid_t parent=getppid();
        for (unsigned i=0;i<500 && getppid()==parent;++i) poll(0,0,10);
        _exit(0);
    }
    tracked=real_child; owned_pid(p,tone)=tracked;
    wait_result=-1; wait_error=ECHILD; real_probes=true; terms=0;
    reap(p,tone); require(owned_pid(p,tone)==real_child,"real live child forgotten under simulated ECHILD");
    stop(p,tone); require(terms==1,"close did not signal its original child");
    bool ended=false;
    for (unsigned i=0;i<1000;++i) {
        const pid_t result=::waitpid(real_child,0,WNOHANG);
        if (result==real_child) { ended=true; break; }
        require(result==0 || (result<0 && errno==EINTR),"real child wait failed");
        poll(0,0,2);
    }
    require(ended,"real child did not terminate");
    reap(p,tone); require(owned_pid(p,tone)<=0,"dead host child was not forgotten");
    real_child=-1; real_probes=false; probe_error=0;
}

static void controller(Plugin& p) {
#if TEST_boost_deck || TEST_shift_deck || TEST_warm_deck || TEST_engine_codes
    require(p.onInit(),"plugin initialization failed");
    wait_result=-1; wait_error=ECHILD; probe_error=0; launches=terms=0;
#if TEST_boost_deck
    bmw::Parcel show(true); p.gaugeVisibility(show);
#elif TEST_engine_codes
    p.startCodes(false);
#else
    bmw::Parcel show(true); p.visibility(show);
#endif
#if TEST_shift_deck
    bmw::Parcel first; first.writeUInt32(p.generation_);
    p.invokeDataUpdate(kShiftPoll,first);
#endif
    const pid_t original=owned_pid(p,false);
    require(original>0 && launches==1,"controller did not start its stand-in");
    for (unsigned i=0;i<10;++i) {
        now_ms+=2500; // pass the existing restart deadline on every tick
#if TEST_boost_deck
        p.poll();
#elif TEST_warm_deck
        p.tick();
#elif TEST_shift_deck
        bmw::Parcel event; event.writeUInt32(p.generation_);
        p.invokeDataUpdate(kShiftPoll,event);
#else
        p.pollCodes(); p.startCodes(false);
#endif
        require(owned_pid(p,false)==original && launches==1,"duplicate worker launched while the first is alive");
    }
#if TEST_boost_deck
    bmw::Parcel hide(false); p.gaugeVisibility(hide);
#elif TEST_engine_codes
    p.stopWorker();
#else
    bmw::Parcel hide(false); p.visibility(hide);
#endif
    require(terms==1 && owned_pid(p,false)==original,"close lost the live PID instead of terminating it");
    owned_pid(p,false)=-1; p.onFinalize();
#else
    (void)p;
#endif
}

int main(int argc, char** argv) {
    const bool controller_only=argc==2 && std::string(argv[1])=="--controller-only";
    require(argc==1 || controller_only,"unknown harness option");
    bmw::IDispatcher dispatcher; bmw::Bundle bundle; bmw::RHMIInterfaceVersion version={1};
    fake_reset();
    Plugin app(dispatcher,bmw::BasicString<char>("Lifecycle test"),1,0,version,bundle,bundle);
    if (!controller_only) {
    state_matrix(app,false); real_process(app,false);
#if TEST_boost_deck
    state_matrix(app,true); real_process(app,true);
#endif
    }
    controller(app);
    require(!real_probes && real_child<0,"host child cleanup incomplete");
    printf("%s: process state matrix, owned-child termination and applicable controller retry/close passed\n",LIFECYCLE_SOURCE);
}
