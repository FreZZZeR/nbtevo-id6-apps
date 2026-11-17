// nbtevo-id6-apps — HMI plugin shell (Apache-2.0).
//
// STAGE D: builds only with the QNX SDP 6.5 + ARM GCC 4.4.2 toolchain against the framework
// libs extracted from the head unit (see README "The two build systems"). A host
// `c++ -fsyntax-only` checks it against the reconstructed ABI header.
//
// The plugin does NO network diagnostics itself — that would block HmiMain3D's UI thread and
// can brick the dash. A separate collector process (collector_main.c) speaks HSFZ/UDS and
// ships decoded frames over loopback UDP; this plugin binds that socket, decodes with the
// shared frame contract, formats with the host-tested hmi_view, and pushes into the HMI
// models. That is the proven ID6_apps split (see NOTICE for the MIT ABI attribution).
#include "sdk/bmw_list.h"

// Our frame/view/xdrive are C — give them C linkage so the Stage-D link resolves against the
// .c translation units (and the POSIX headers while we're here). bmw_list.h stays C++ above.
extern "C" {
#include "frame.h"
#include "hmi_view.h"
#include "xdrive.h"   // ND_XDRIVE_RWD / ND_XDRIVE_AWD
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
}

// ABI sizes we depend on — asserted only on the real ARM cross-build.
#if defined(__arm__)
typedef char abi_pluginapp_size[(sizeof(bmw::PluginApp) == 352) ? 1 : -1];
typedef char abi_string_size[(sizeof(bmw::BasicString<char>) == 4) ? 1 : -1];
#endif

namespace {
const unsigned kFrameUdpPort = 42861;   // must match collector_main.c ND_PLUGIN_UDP_PORT
// Model / action IDs — MUST match native/plugin/nbtevo-diag.xml (the DescFile). The manifest's
// connector id 120 is clear of ID6_apps and the NBT-AAD plugin (100/103/104).
const unsigned MODEL_GAUGE0      = 50;   // gauge slot N -> model 50+N (coolant..trans = 50..57)
const unsigned MODEL_CODES       = 60;   // fault-codes summary label
const unsigned MODEL_XDRIVE      = 70;   // xDrive status label
const unsigned ACTION_XDRIVE_RWD = 21;   // raAction: "Rear-wheel drive" button
const unsigned ACTION_XDRIVE_AWD = 22;   // raAction: "All-wheel drive" button
}

class NbtevoDiagApp : public bmw::PluginApp {
    int      fd_;        // loopback UDP from the collector
    nd_frame frame_;     // last snapshot
    bool     have_;
public:
    NbtevoDiagApp(bmw::IDispatcher& d, bmw::BasicString<char> name, unsigned maj, unsigned mn,
                  bmw::RHMIInterfaceVersion rhmi, const bmw::Bundle& cfg, const bmw::Bundle& res)
        : bmw::PluginApp(d, name, maj, mn, rhmi, cfg, res), fd_(-1), have_(false)
    { memset(&frame_, 0, sizeof frame_); }

    virtual ~NbtevoDiagApp() { if (fd_ >= 0) close(fd_); }

    // xDrive menu actions -> our handlers (wired via Delegate in onInit)
    bool onEngageRwd(const bmw::Parcel&) { sendXdriveCommand(ND_XDRIVE_RWD); return true; }
    bool onRestoreAwd(const bmw::Parcel&) { sendXdriveCommand(ND_XDRIVE_AWD); return true; }

    virtual bool onInit() {
        struct sockaddr_in a;
        fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        memset(&a, 0, sizeof a);
        a.sin_family = AF_INET;
        a.sin_port = htons((unsigned short)kFrameUdpPort);
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (fd_ >= 0) (void)bind(fd_, (struct sockaddr *)&a, sizeof a);
        // TODO(Stage C): fork/exec the collector (collector_main.c) and reap it on finalize,
        //                exactly as ID6_apps spawns its boostdeck-collector-qnx.

        bmw::Delegate<bool, const bmw::Parcel&> rwd;
        rwd.m_obj  = this;
        rwd.m_exec = &bmw::Delegate<bool, const bmw::Parcel&>::execImpl<NbtevoDiagApp, &NbtevoDiagApp::onEngageRwd>;
        addHandler(ACTION_XDRIVE_RWD, rwd);

        bmw::Delegate<bool, const bmw::Parcel&> awd;
        awd.m_obj  = this;
        awd.m_exec = &bmw::Delegate<bool, const bmw::Parcel&>::execImpl<NbtevoDiagApp, &NbtevoDiagApp::onRestoreAwd>;
        addHandler(ACTION_XDRIVE_AWD, awd);

        poll();   // first read
        return true;
    }

    virtual bool onFinalize() { if (fd_ >= 0) { close(fd_); fd_ = -1; } return true; }

    // Pull any pending frame(s) off the socket and repaint. Runs on the UI thread.
    void poll() {
        unsigned char buf[ND_FRAME_WIRE];
        while (fd_ >= 0) {
            ssize_t r = recv(fd_, buf, sizeof buf, MSG_DONTWAIT);
            if (r < (ssize_t)ND_FRAME_WIRE) break;
            nd_frame f;
            if (nd_frame_decode(buf, (int)r, &f)) { frame_ = f; have_ = true; }
        }
        if (have_) refresh();
        // TODO(Stage C/D): reschedule via post(Runnable*, intervalMs) on the dispatcher.
    }

    // Format the frame with the host-tested hmi_view and push into the HMI list/label models.
    void refresh() {
        char line[64];
        int slot;
        for (slot = 0; slot < ND_NGAUGE; slot++) {
            nd_fmt_gauge(&frame_, slot, line, (int)sizeof line);
            updateModel(MODEL_GAUGE0 + (unsigned)slot, bmw::BasicString<char>(line));
        }
        nd_fmt_codes(&frame_, line, (int)sizeof line);
        updateModel(MODEL_CODES, bmw::BasicString<char>(line));
        nd_fmt_xdrive(&frame_, line, (int)sizeof line);
        updateModel(MODEL_XDRIVE, bmw::BasicString<char>(line));
    }

private:
    // TODO(Stage C): send the actuation request to the collector over a small reverse channel
    // (it owns the gateway lease and runs nd_xdrive_set with the code-enforced fuse). The HMI
    // plugin must never drive the DSC directly from HmiMain3D.
    void sendXdriveCommand(int target) { (void)target; }
};

// --- loader export ------------------------------------------------------------------------
// TODO(Stage D): confirm the exact export symbol/struct the HMI loader scans for in the
// aftermarket plugin directory (ID6_apps uses a PluginAppInfo-style descriptor).
struct NbtevoPluginInfo {
    unsigned abiVersion;
    const char* name;
    const char* vendor;
    bmw::PluginApp* (*create)(bmw::IDispatcher&, const char*, const bmw::Bundle&, const bmw::Bundle&);
};

static bmw::PluginApp* create_app(bmw::IDispatcher& d, const char* name,
                                  const bmw::Bundle& cfg, const bmw::Bundle& res) {
    bmw::RHMIInterfaceVersion v = { 1 };
    return new NbtevoDiagApp(d, bmw::BasicString<char>(name), 1u, 0u, v, cfg, res);
}

extern "C" {
NbtevoPluginInfo nbtevo_diag_plugin = { 1u, "nbtevo-diag", "nbtevo", &create_app };
}
