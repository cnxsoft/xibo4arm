/*  Berkelium - Embedded Chromium
 *  ForkedProcessHook.cpp
 *
 *  Copyright (c) 2010, Patrick Reiter Horn
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Sirikata nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "content/browser/tab_contents/tab_contents.h"
#include "berkelium/Berkelium.hpp"
#include "Root.hpp"


////////////// Chrome Main function /////////////
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(port): the ifdefs in here are a first step towards trying to determine
// the correct abstraction for all the OS functionality required at this
// stage of process initialization. It should not be taken as a final
// abstraction.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <algorithm>
#include <atlbase.h>
#include <atlapp.h>
#include <malloc.h>
#include <new.h>
#elif defined(OS_POSIX)
#include <locale.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(USE_X11)
#include <dbus/dbus-glib.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "app/app_paths.h"
#include "app/app_switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "views/debug_utils.h"
#include "base/i18n/icu_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/diagnostics/diagnostics_main.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "chrome/browser/platform_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/sandbox_init_wrapper.h"
#include "chrome/common/url_constants.h"
#include "ipc/ipc_switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#if defined(USE_NSS)
#include "base/nss_util.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_LINUX)
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#include "content/browser/zygote_host_linux.h"
#endif

#if defined(OS_MACOSX)
#include "ui/base/l10n/l10n_util_mac.h"
#include "base/mac/mac_util.h"
#include "base/mac/os_crash_dumps.h"
#include "base/mach_ipc_mac.h"
#include "chrome/app/breakpad_mac.h"
#include "chrome/browser/mach_broker_mac.h"
#include "chrome/common/chrome_paths_internal.h"
#include "grit/chromium_strings.h"
#include "third_party/WebKit/Source/WebKit/mac/WebCoreSupport/WebSystemInterface.h"
#endif

#if defined(OS_POSIX)
#include "base/global_descriptors_posix.h"
#endif

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "sandbox/src/dep.h"
#include "sandbox/src/sandbox.h"
#include "tools/memory_watcher/memory_watcher.h"
#endif

#if defined(USE_TCMALLOC)
#include "third_party/tcmalloc/chromium/src/google/malloc_extension.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_loader.h"
#endif

extern int BrowserMain(const MainFunctionParams&);
extern int RendererMain(const MainFunctionParams&);
extern int GpuMain(const MainFunctionParams&);
extern int PluginMain(const MainFunctionParams&);
extern int WorkerMain(const MainFunctionParams&);
extern int NaClMain(const MainFunctionParams&);
extern int UtilityMain(const MainFunctionParams&);
extern int ProfileImportMain(const MainFunctionParams&);
extern int ZygoteMain(const MainFunctionParams&);
#if defined(_WIN64)
extern int NaClBrokerMain(const MainFunctionParams&);
#endif
extern int ServiceProcessMain(const MainFunctionParams&);

#if defined(OS_WIN)
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

namespace {

#if defined(OS_WIN)
const wchar_t kProfilingDll[] = L"memory_watcher.dll";

// Load the memory profiling DLL.  All it needs to be activated
// is to be loaded.  Return true on success, false otherwise.
bool LoadMemoryProfiler() {
  HMODULE prof_module = LoadLibrary(kProfilingDll);
  return prof_module != NULL;
}

CAppModule _Module;

#pragma optimize("", off)
// Handlers for invalid parameter and pure call. They generate a breakpoint to
// tell breakpad that it needs to dump the process.
void InvalidParameter(const wchar_t* expression, const wchar_t* function,
                      const wchar_t* file, unsigned int line,
                      uintptr_t reserved) {
  __debugbreak();
}

void PureCall() {
  __debugbreak();
}

#pragma warning(push)
// Disables warning 4748 which is: "/GS can not protect parameters and local
// variables from local buffer overrun because optimizations are disabled in
// function."  GetStats() will not overflow the passed-in buffer and this
// function never returns.
#pragma warning(disable : 4748)
void OnNoMemory() {
#if defined(USE_TCMALLOC)
  // Try to get some information on the stack to make the crash easier to
  // diagnose from a minidump, being very careful not to do anything that might
  // try to heap allocate.
  char buf[32*1024];
  MallocExtension::instance()->GetStats(buf, sizeof(buf));
#endif
  // Kill the process. This is important for security, since WebKit doesn't
  // NULL-check many memory allocations. If a malloc fails, returns NULL, and
  // the buffer is then used, it provides a handy mapping of memory starting at
  // address 0 for an attacker to utilize.
  __debugbreak();
}
#pragma warning(pop)

// Handlers to silently dump the current process when there is an assert in
// chrome.
void ChromeAssert(const std::string& str) {
  // Get the breakpad pointer from chrome.exe
  typedef void (__cdecl *DumpProcessFunction)();
  DumpProcessFunction DumpProcess = reinterpret_cast<DumpProcessFunction>(
      ::GetProcAddress(::GetModuleHandle(chrome::kBrowserProcessExecutableName),
                       "DumpProcess"));
  if (DumpProcess)
    DumpProcess();
}

#pragma optimize("", on)

// Early versions of Chrome incorrectly registered a chromehtml: URL handler,
// which gives us nothing but trouble. Avoid launching chrome this way since
// some apps fail to properly escape arguments.
bool HasDeprecatedArguments(const std::wstring& command_line) {
  const wchar_t kChromeHtml[] = L"chromehtml:";
  std::wstring command_line_lower = command_line;
  // We are only searching for ASCII characters so this is OK.
  StringToLowerASCII(&command_line_lower);
  std::wstring::size_type pos = command_line_lower.find(kChromeHtml);
  return (pos != std::wstring::npos);
}

#endif  // defined(OS_WIN)

#if defined(USE_X11)
static void GLibLogHandler(const gchar* log_domain,
                           GLogLevelFlags log_level,
                           const gchar* message,
                           gpointer userdata) {
  if (!log_domain)
    log_domain = "<unknown>";
  if (!message)
    message = "<no message>";

  if (strstr(message, "Loading IM context type") ||
      strstr(message, "wrong ELF class: ELFCLASS64")) {
    // http://crbug.com/9643
    // Until we have a real 64-bit build or all of these 32-bit package issues
    // are sorted out, don't fatal on ELF 32/64-bit mismatch warnings and don't
    // spam the user with more than one of them.
    static bool alerted = false;
    if (!alerted) {
      LOG(ERROR) << "Bug 9643: " << log_domain << ": " << message;
      alerted = true;
    }
  } else if (strstr(message, "gtk_widget_size_allocate(): attempt to "
                             "allocate widget with width") &&
             !GTK_CHECK_VERSION(2, 16, 1)) {
    // This warning only occurs in obsolete versions of GTK and is harmless.
    // http://crbug.com/11133
  } else if (strstr(message, "Theme file for default has no") ||
             strstr(message, "Theme directory") ||
             strstr(message, "theme pixmap")) {
    LOG(ERROR) << "GTK theme error: " << message;
  } else if (strstr(message, "gtk_drag_dest_leave: assertion")) {
    LOG(ERROR) << "Drag destination deleted: http://crbug.com/18557";
  } else {
#ifdef NDEBUG
    LOG(ERROR) << log_domain << ": " << message;
#else
    LOG(FATAL) << log_domain << ": " << message;
#endif
  }
}

static void SetUpGLibLogHandler() {
  // Register GLib-handled assertions to go through our logging system.
  const char* kLogDomains[] = { NULL, "Gtk", "Gdk", "GLib", "GLib-GObject" };
  for (size_t i = 0; i < arraysize(kLogDomains); i++) {
    g_log_set_handler(kLogDomains[i],
                      static_cast<GLogLevelFlags>(G_LOG_FLAG_RECURSION |
                                                  G_LOG_FLAG_FATAL |
                                                  G_LOG_LEVEL_ERROR |
                                                  G_LOG_LEVEL_CRITICAL |
                                                  G_LOG_LEVEL_WARNING),
                      GLibLogHandler,
                      NULL);
  }
}
#endif  // defined(USE_X11)

#if defined(OS_LINUX)
static void AdjustLinuxOOMScore(const std::string& process_type) {
  const int kMiscScore = 7;
  const int kPluginScore = 10;
  int score = -1;

  if (process_type == switches::kPluginProcess) {
    score = kPluginScore;
  } else if (process_type == switches::kUtilityProcess ||
             process_type == switches::kWorkerProcess ||
             process_type == switches::kGpuProcess ||
             process_type == switches::kServiceProcess) {
    score = kMiscScore;
  } else if (process_type == switches::kProfileImportProcess) {
    NOTIMPLEMENTED();
#ifndef DISABLE_NACL
  } else if (process_type == switches::kNaClLoaderProcess) {
    score = kPluginScore;
#endif
  } else if (process_type == switches::kZygoteProcess ||
             process_type.empty()) {
    // Pass - browser / zygote process stays at 0.
  } else if (process_type == switches::kExtensionProcess ||
             process_type == switches::kRendererProcess) {
    LOG(WARNING) << "process type '" << process_type << "' "
                 << "should go through the zygote.";
    // When debugging, these process types can end up being run directly.
    return;
  } else {
    NOTREACHED() << "Unknown process type";
  }
  if (score > -1)
    base::AdjustOOMScore(base::GetCurrentProcId(), score);
}
#endif  // defined(OS_LINUX)

// Register the invalid param handler and pure call handler to be able to
// notify breakpad when it happens.
void RegisterInvalidParamHandler() {
#if defined(OS_WIN)
  _set_invalid_parameter_handler(InvalidParameter);
  _set_purecall_handler(PureCall);
  // Gather allocation failure.
  std::set_new_handler(&OnNoMemory);
  // Also enable the new handler for malloc() based failures.
  _set_new_mode(1);
#endif
}

void SetupCRT(const CommandLine& parsed_command_line) {
#if defined(OS_WIN)
#ifdef _CRTDBG_MAP_ALLOC
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#else
  if (!parsed_command_line.HasSwitch(switches::kDisableBreakpad)) {
    _CrtSetReportMode(_CRT_ASSERT, 0);
  }
#endif

#endif
}

// Enable the heap profiler if the appropriate command-line switch is
// present, bailing out of the app we can't.
void EnableHeapProfiler(const CommandLine& parsed_command_line) {
#if defined(OS_WIN)
  if (parsed_command_line.HasSwitch(switches::kMemoryProfiling))
    if (!LoadMemoryProfiler())
      exit(-1);
#endif
}

void CommonSubprocessInit() {
#if defined(OS_WIN)
  // HACK: Let Windows know that we have started.  This is needed to suppress
  // the IDC_APPSTARTING cursor from being displayed for a prolonged period
  // while a subprocess is starting.
  PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
  MSG msg;
  PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
#endif
}

// Returns true if this subprocess type needs the ResourceBundle initialized
// and resources loaded.
bool SubprocessNeedsResourceBundle(const std::string& process_type) {
  return
#if defined(OS_WIN) || defined(OS_MACOSX)
      // Windows needs resources for the default/null plugin.
      // Mac needs them for the plugin process name.
      process_type == switches::kPluginProcess ||
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX)
      // The zygote process opens the resources for the renderers.
      process_type == switches::kZygoteProcess ||
#endif
      process_type == switches::kRendererProcess ||
      process_type == switches::kExtensionProcess ||
      process_type == switches::kUtilityProcess;
}

// Returns true if this process is a child of the browser process.
bool SubprocessIsBrowserChild(const std::string& process_type) {
  if (process_type.empty() || process_type == switches::kServiceProcess) {
    return false;
  }
  return true;
}

#if defined(OS_MACOSX)
// Update the name shown in Activity Monitor so users are less likely to ask
// why Chrome has so many processes.
void SetMacProcessName(const std::string& process_type) {
  // Don't worry about the browser process, its gets the stock name.
  int name_id = 0;
  if (process_type == switches::kRendererProcess) {
    name_id = IDS_RENDERER_APP_NAME;
  } else if (process_type == switches::kPluginProcess) {
    name_id = IDS_PLUGIN_APP_NAME;
  } else if (process_type == switches::kExtensionProcess) {
    name_id = IDS_WORKER_APP_NAME;
  } else if (process_type == switches::kUtilityProcess) {
    name_id = IDS_UTILITY_APP_NAME;
  }
  if (name_id) {
    NSString* app_name = l10n_util::GetNSString(name_id);
    base::mac::SetProcessName(reinterpret_cast<CFStringRef>(app_name));
  }
}

// Completes the Mach IPC handshake by sending this process' task port to the
// parent process.  The parent is listening on the Mach port given by
// |GetMachPortName()|.  The task port is used by the parent to get CPU/memory
// stats to display in the task manager.
void SendTaskPortToParentProcess() {
  const mach_msg_timeout_t kTimeoutMs = 100;
  const int32_t kMessageId = 0;
  std::string mach_port_name = MachBroker::GetMachPortName();

  base::MachSendMessage child_message(kMessageId);
  if (!child_message.AddDescriptor(mach_task_self())) {
    LOG(ERROR) << "child AddDescriptor(mach_task_self()) failed.";
    return;
  }

  base::MachPortSender child_sender(mach_port_name.c_str());
  kern_return_t err = child_sender.SendMessage(child_message, kTimeoutMs);
  if (err != KERN_SUCCESS) {
    LOG(ERROR) << StringPrintf("child SendMessage() failed: 0x%x %s", err,
                               mach_error_string(err));
  }
}
#endif  // defined(OS_MACOSX)

void InitializeStatsTable(base::ProcessId browser_pid,
                          const CommandLine & parsed_command_line) {
  // Initialize the Stats Counters table.  With this initialized,
  // the StatsViewer can be utilized to read counters outside of
  // Chrome.  These lines can be commented out to effectively turn
  // counters 'off'.  The table is created and exists for the life
  // of the process.  It is not cleaned up.
}

}  // namespace

namespace Berkelium {
// NOTE: forkedProcessHook runs in a different process and should not access
// any non-static Berkelium data, such as Root's singleton.

#if defined(OS_WIN)
void forkedProcessHook(
    sandbox::BrokerServices* (*ptrGetBrokerServices)(),
    sandbox::TargetServices* (*ptrGetTargetServices)(),
    bool (*ptrSetCurrentProcessDEP)(enum sandbox::DepEnforcement))
{
#else
void forkedProcessHook(int argc, char **argv)
{
#endif

#if defined(OS_MACOSX)
  // TODO(mark): Some of these things ought to be handled in chrome_exe_main.mm.
  // Under the current architecture, nothing in chrome_exe_main can rely
  // directly on chrome_dll code on the Mac, though, so until some of this code
  // is refactored to avoid such a dependency, it lives here.  See also the
  // TODO(mark) below at InitCrashReporter() and DestructCrashReporter().
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
#endif  // OS_MACOSX

  RegisterInvalidParamHandler();

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  // We need this pool for all the objects created before we get to the
  // event loop, but we don't want to leave them hanging around until the
  // app quits. Each "main" needs to flush this pool right before it goes into
  // its main event loop to get rid of the cruft.
  base::mac::ScopedNSAutoreleasePool autorelease_pool;

#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->SaveChromeMainStats();
#endif

#if defined(OS_POSIX)
  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  g_fds->Set(kPrimaryIPCChannel,
             kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor);
#if defined(OS_LINUX)
  g_fds->Set(kCrashDumpSignal,
             kCrashDumpSignal + base::GlobalDescriptors::kBaseDescriptor);
#endif
#endif

#if defined(OS_POSIX)
  // Set C library locale to make sure CommandLine can parse argument values
  // in correct encoding.
  setlocale(LC_ALL, "");

  // Sanitise our signal handling state. Signals that were ignored by our
  // parent will also be ignored by us. We also inherit our parent's sigmask.
  sigset_t empty_signal_set;
  CHECK(0 == sigemptyset(&empty_signal_set));
  CHECK(0 == sigprocmask(SIG_SETMASK, &empty_signal_set, NULL));

  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  static const int signals_to_reset[] =
      {SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE, SIGSEGV,
       SIGALRM, SIGTERM, SIGCHLD, SIGBUS, SIGTRAP};  // SIGPIPE is set below.
  for (unsigned i = 0; i < arraysize(signals_to_reset); i++) {
    CHECK(0 == sigaction(signals_to_reset[i], &sigact, NULL));
  }
#endif

  // Initialize the command line.
#if defined(OS_WIN)
  CommandLine::Init(0, NULL);
#else
  CommandLine::Init(argc, argv);
#endif

  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  const chrome::VersionInfo version_info;

#if defined(OS_POSIX)

#if !defined(OS_MACOSX)
  if (parsed_command_line.HasSwitch(switches::kProductVersion)) {
    printf("%s\n", version_info.Version().c_str());
    return;
  }
#endif

  if (parsed_command_line.HasSwitch(switches::kVersion)) {
    printf("%s %s %s\n",
           version_info.Name().c_str(),
           version_info.Version().c_str(),
           platform_util::GetVersionStringModifier().c_str());
    return;
  }
#endif  // OS_POSIX

  std::string process_type =
      parsed_command_line.GetSwitchValueASCII(switches::kProcessType);

#if defined(OS_MACOSX)
  base::mac::SetOverrideAppBundlePath(chrome::GetFrameworkBundlePath());
#endif  // OS_MACOSX

  // If we are in diagnostics mode this is the end of the line. After the
  // diagnostics are run the process will invariably exit.
  if (parsed_command_line.HasSwitch(switches::kDiagnostics)) {
    DiagnosticsMain(parsed_command_line);
    return;
  }

#if defined(OS_WIN)
  // Must do this before any other usage of command line!
  if (HasDeprecatedArguments(parsed_command_line.command_line_string()))
    return;
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  // Show the man page on --help or -h.
  if (parsed_command_line.HasSwitch(switches::kHelp) ||
      parsed_command_line.HasSwitch(switches::kHelpShort)) {
    FilePath binary(parsed_command_line.argv()[0]);
    execlp("man", "man", binary.BaseName().value().c_str(), NULL);
    PLOG(FATAL) << "execlp failed";
  }
#endif

#if defined(OS_POSIX)
  // Always ignore SIGPIPE.  We check the return value of write().
  CHECK(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
#endif  // OS_POSIX

  if (parsed_command_line.HasSwitch(switches::kEnableNaCl)) {
    // NaCl currently requires two flags to run
    CommandLine* singleton_command_line = CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kEnableGPUPlugin);
  }

#if defined(OS_CHROMEOS)
  if (parsed_command_line.HasSwitch(switches::kGuestSession)) {
    // Disable sync and extensions if we're in "browse without sign-in" mode.
    CommandLine* singleton_command_line = CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kDisableSync);
    singleton_command_line->AppendSwitch(switches::kDisableExtensions);
    browser_defaults::bookmarks_enabled = false;
  }
#endif

  base::ProcessId browser_pid = base::GetCurrentProcId();
  if (SubprocessIsBrowserChild(process_type)) {
#if defined(OS_WIN)
    std::string channel_name =
        parsed_command_line.GetSwitchValueASCII(switches::kProcessChannelID);

    int browser_pid_int;
    base::StringToInt(channel_name, &browser_pid_int);
    browser_pid = static_cast<base::ProcessId>(browser_pid_int);
    DCHECK_NE(browser_pid, 0u);
#elif defined(OS_MACOSX)
    browser_pid = base::GetCurrentProcId();
    SendTaskPortToParentProcess();
#elif defined(OS_POSIX)
    // On linux, we're in the zygote here; so we need the parent process' id.
    browser_pid = base::GetParentProcessId(base::GetCurrentProcId());
#endif

#if defined(OS_POSIX)
    // When you hit Ctrl-C in a terminal running the browser
    // process, a SIGINT is delivered to the entire process group.
    // When debugging the browser process via gdb, gdb catches the
    // SIGINT for the browser process (and dumps you back to the gdb
    // console) but doesn't for the child processes, killing them.
    // The fix is to have child processes ignore SIGINT; they'll die
    // on their own when the browser process goes away.
    //
    // Note that we *can't* rely on BeingDebugged to catch this case because we
    // are the child process, which is not being debugged.
    if (!base::debug::BeingDebugged())
      signal(SIGINT, SIG_IGN);
#endif
  }
  SetupCRT(parsed_command_line);

  // Initialize the Chrome path provider.
  app::RegisterPathProvider();
  chrome::RegisterPathProvider();
  ui::RegisterPathProvider();

  // Notice a user data directory override if any
  const FilePath user_data_dir =
      parsed_command_line.GetSwitchValuePath(switches::kUserDataDir);
  if (!user_data_dir.empty())
    CHECK(PathService::Override(chrome::DIR_USER_DATA, user_data_dir));

#if defined(OS_MACOSX)
  // TODO(mark): Right now, InitCrashReporter() needs to be called after
  // CommandLine::Init() and chrome::RegisterPathProvider().  Ideally, Breakpad
  // initialization could occur sooner, preferably even before the framework
  // dylib is even loaded, to catch potential early crashes.
  InitCrashReporter();

#if defined(NDEBUG)
  bool is_debug_build = false;
#else
  bool is_debug_build = true;
#endif

  // Details on when we enable Apple's Crash reporter.
  //
  // Motivation:
  //    In debug mode it takes Apple's crash reporter eons to generate a crash
  // dump.
  //
  // What we do:
  // * We only pass crashes for foreground processes to Apple's Crash reporter.
  //    At the time of this writing, that means just the Browser process.
  // * If Breakpad is enabled, it will pass browser crashes to Crash Reporter
  //    itself.
  // * If Breakpad is disabled, we only turn on Crash Reporter for the
  //    Browser process in release mode.
  if (!parsed_command_line.HasSwitch(switches::kDisableBreakpad)) {
    bool disable_apple_crash_reporter = is_debug_build
                                        || base::mac::IsBackgroundOnlyProcess();
    if (!IsCrashReporterEnabled() && disable_apple_crash_reporter) {
      base::mac::DisableOSCrashDumps();
    }
  }

#if defined(OS_MACOSX)
  // Mac Chrome is packaged with a main app bundle and a helper app bundle.
  // The main app bundle should only be used for the browser process, so it
  // should never see a --type switch (switches::kProcessType).  Likewise,
  // the helper should always have a --type switch.
  //
  // This check is done this late so there is already a call to
  // mac_util::IsBackgroundOnlyProcess(), so there is no change in
  // startup/initialization order.

  // The helper's Info.plist marks it as a background only app.
  CHECK(parsed_command_line.HasSwitch(switches::kProcessType))
      << "Berkelium helper application requires --type.";
#endif  // defined(OS_MACOSX)

  if (IsCrashReporterEnabled())
    InitCrashProcessInfo();
#endif  // OS_MACOSX

  InitializeStatsTable(browser_pid, parsed_command_line);

  // Enable the heap profiler as early as possible!
  EnableHeapProfiler(parsed_command_line);

  // Enable Message Loop related state asap.
  if (parsed_command_line.HasSwitch(switches::kMessageLoopHistogrammer))
    MessageLoop::EnableHistogrammer(true);

  // Checks if the sandbox is enabled in this process and initializes it if this
  // is the case. The crash handler depends on this so it has to be done before
  // its initialization.
  SandboxInitWrapper sandbox_wrapper;
#if defined(OS_WIN)
  base::win::Version win_version = base::win::GetVersion();
  if (win_version <		base::win::VERSION_VISTA) {
    // On Vista, this is unnecessary since it is controlled through the
    // /NXCOMPAT linker flag.
    // Enforces strong DEP support.
    (*ptrSetCurrentProcessDEP)(sandbox::DEP_ENABLED);
  }

  // Get the interface pointer to the BrokerServices or TargetServices,
  // depending who we are.
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  sandbox_info.broker_services = (*ptrGetBrokerServices)();
  if (!sandbox_info.broker_services)
    sandbox_info.target_services = (*ptrGetTargetServices)();
  sandbox_wrapper.SetServices(&sandbox_info);
#endif

  // OS X enables sandboxing later in the startup process.
#if !defined (OS_MACOSX)
  sandbox_wrapper.InitializeSandbox(parsed_command_line, process_type);
#endif  // !OS_MACOSX

#if defined(OS_WIN)
  _Module.Init(NULL, HINST_THISCOMPONENT);
#endif

  bool single_process =
#if defined (GOOGLE_CHROME_BUILD)
    // This is an unsupported and not fully tested mode, so don't enable it for
    // official Chrome builds.
    false;
#else
    parsed_command_line.HasSwitch(switches::kSingleProcess);
#endif
  if (single_process)
    RenderProcessHost::set_run_renderer_in_process(true);
#if defined(OS_MACOSX)
  // TODO(port-mac): This is from renderer_main_platform_delegate.cc.
  // shess tried to refactor things appropriately, but it sprawled out
  // of control because different platforms needed different styles of
  // initialization.  Try again once we understand the process
  // architecture needed and where it should live.
  if (single_process)
    InitWebCoreSystemInterface();
#endif

  bool icu_result = icu_util::Initialize();
  CHECK(icu_result);

  logging::OldFileDeletionState file_state =
      logging::APPEND_TO_OLD_LOG_FILE;
  if (process_type.empty()) {
    file_state = logging::DELETE_OLD_LOG_FILE;
  }
  logging::InitChromeLogging(parsed_command_line, file_state);

  // Register internal Chrome schemes so they'll be parsed correctly. This must
  // happen before we process any URLs with the affected schemes, and must be
  // done in all processes that work with these URLs (i.e. including renderers).
  chrome::RegisterChromeSchemes();

#ifdef NDEBUG
  if (parsed_command_line.HasSwitch(switches::kSilentDumpOnDCHECK) &&
      parsed_command_line.HasSwitch(switches::kEnableDCHECK)) {
#if defined(OS_WIN)
    logging::SetLogReportHandler(ChromeAssert);
#endif
  }
#endif  // NDEBUG

  if (SubprocessNeedsResourceBundle(process_type)) {
    // Initialize ResourceBundle which handles files loaded from external
    // sources.  The language should have been passed in to us from the
    // browser process as a command line flag.
    DCHECK(parsed_command_line.HasSwitch(switches::kLang) ||
           process_type == switches::kZygoteProcess);

    // TODO(markusheintz): The command line flag --lang is actually processed
    // by the CommandLinePrefStore, and made available through the PrefService
    // via the preference prefs::kApplicationLocale. The browser process uses
    // the --lang flag to passe the value of the PrefService in here. Maybe this
    // value could be passed in a different way.
    ResourceBundle::InitSharedInstance(
        parsed_command_line.GetSwitchValueASCII(switches::kLang));

#if defined(OS_MACOSX)
    // Update the process name (need resources to get the strings, so
    // only do this when ResourcesBundle has been initialized).
    SetMacProcessName(process_type);
#endif  // defined(OS_MACOSX)
  }

  if (!process_type.empty())
    CommonSubprocessInit();

#if defined(OS_MACOSX)
  // On OS X the renderer sandbox needs to be initialized later in the startup
  // sequence in RendererMainPlatformDelegate::EnableSandbox().
  // Same goes for NaClLoader, in NaClMainPlatformDelegate::EnableSandbox().
  if (process_type != switches::kRendererProcess &&
      process_type != switches::kExtensionProcess &&
      process_type != switches::kNaClLoaderProcess) {
    bool sandbox_initialized_ok =
        sandbox_wrapper.InitializeSandbox(parsed_command_line, process_type);
    // Die if the sandbox can't be enabled.
    CHECK(sandbox_initialized_ok) << "Error initializing sandbox for "
                                  << process_type;
  }
#endif  // OS_MACOSX

  MainFunctionParams main_params(parsed_command_line, sandbox_wrapper,
                                 &autorelease_pool);

  // Note: If you are adding a new process type below, be sure to adjust the
  // AdjustLinuxOOMScore function too.
#if defined(OS_LINUX)
  AdjustLinuxOOMScore(process_type);
  // TODO(mdm): look into calling CommandLine::SetProcTitle() here instead of
  // in each relevant main() function below, to fix /proc/self/exe showing up
  // as our process name since we exec() via that to be update-safe.
#endif

  // TODO(port): turn on these main() functions as they've been de-winified.
  int rv = -1;
  if (process_type == switches::kRendererProcess) {
    rv = RendererMain(main_params);
  } else if (process_type == switches::kExtensionProcess) {
    // An extension process is just a renderer process. We use a different
    // command line argument to differentiate crash reports.
    rv = RendererMain(main_params);
  } else if (process_type == switches::kPluginProcess) {
    rv = PluginMain(main_params);
  } else if (process_type == switches::kUtilityProcess) {
    rv = UtilityMain(main_params);
  } else if (process_type == switches::kGpuProcess) {
    rv = GpuMain(main_params);
  } else if (process_type == switches::kProfileImportProcess) {
#if defined(OS_MACOSX)
    rv = ProfileImportMain(main_params);
#else
    // TODO(port): Use OOP profile import - http://crbug.com/22142 .
    NOTIMPLEMENTED();
    rv = -1;
#endif
  } else if (process_type == switches::kWorkerProcess) {
    rv = WorkerMain(main_params);
#ifndef DISABLE_NACL
  } else if (process_type == switches::kNaClLoaderProcess) {
    rv = NaClMain(main_params);
#endif
#ifdef _WIN64  // The broker process is used only on Win64.
  } else if (process_type == switches::kNaClBrokerProcess) {
    rv = NaClBrokerMain(main_params);
#endif
  } else if (process_type == switches::kZygoteProcess) {
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    // This function call can return multiple times, once per fork().
    if (ZygoteMain(main_params)) {
      // Zygote::HandleForkRequest may have reallocated the command
      // line so update it here with the new version.
      const CommandLine& parsed_command_line =
        *CommandLine::ForCurrentProcess();

      // The StatsTable must be initialized in each process; we already
      // initialized for the browser process, now we need to initialize
      // within the new processes as well.
      InitializeStatsTable(browser_pid, parsed_command_line);

      MainFunctionParams main_params(parsed_command_line, sandbox_wrapper,
                                     &autorelease_pool);
      std::string process_type =
        parsed_command_line.GetSwitchValueASCII(switches::kProcessType);
      if (process_type == switches::kRendererProcess ||
          process_type == switches::kExtensionProcess) {
        rv = RendererMain(main_params);
#if !defined(DISABLE_NACL)
      } else if (process_type == switches::kNaClLoaderProcess) {
        rv = NaClMain(main_params);
#endif
      } else if (process_type == switches::kWorkerProcess) {
        rv = WorkerMain(main_params);
      } else {
        NOTREACHED() << "Unknown process type: " << process_type;
      }
    } else {
      rv = 0;
    }
#else
    NOTIMPLEMENTED();
#endif
  } else if (process_type == switches::kServiceProcess) {
    rv = ServiceProcessMain(main_params);
  } else if (process_type.empty()) {
#if defined(OS_LINUX)
    const char* sandbox_binary = NULL;
    struct stat st;

    // In Chromium branded builds, developers can set an environment variable to
    // use the development sandbox. See
    // http://code.google.com/p/chromium/wiki/LinuxSUIDSandboxDevelopment
    if (stat("/proc/self/exe", &st) == 0 && st.st_uid == getuid())
      sandbox_binary = getenv("CHROME_DEVEL_SANDBOX");

#if defined(LINUX_SANDBOX_PATH)
    if (!sandbox_binary)
      sandbox_binary = LINUX_SANDBOX_PATH;
#endif

    std::string sandbox_cmd;
    if (sandbox_binary && !parsed_command_line.HasSwitch(switches::kNoSandbox))
      sandbox_cmd = sandbox_binary;

    // Tickle the sandbox host and zygote host so they fork now.
    RenderSandboxHostLinux* shost = RenderSandboxHostLinux::GetInstance();
    shost->Init(sandbox_cmd);
    ZygoteHost* zhost = ZygoteHost::GetInstance();
    zhost->Init(sandbox_cmd);

#if defined(USE_NSS)
    // We want to be sure to init NSPR on the main thread.
    base::EnsureNSPRInit();
#endif

    g_thread_init(NULL);
    // Glib type system initialization. Needed at least for gconf,
    // used in net/proxy/proxy_config_service_linux.cc. Most likely
    // this is superfluous as gtk_init() ought to do this. It's
    // definitely harmless, so retained as a reminder of this
    // requirement for gconf.
    g_type_init();
    // We use glib-dbus for geolocation and it's possible other libraries
    // (e.g. gnome-keyring) will use it, so initialize its threading here
    // as well.
    dbus_g_thread_init();
    // gtk_init() can change |argc| and |argv|.
    gtk_init(&argc, &argv);
    SetUpGLibLogHandler();

    ui::SetDefaultX11ErrorHandlers();
#endif  // defined(OS_LINUX)

    rv = BrowserMain(main_params);
  } else {
    NOTREACHED() << "Unknown process type";
  }

  if (SubprocessNeedsResourceBundle(process_type))
    ResourceBundle::CleanupSharedInstance();

#if defined(OS_WIN)
#ifdef _CRTDBG_MAP_ALLOC
  _CrtDumpMemoryLeaks();
#endif  // _CRTDBG_MAP_ALLOC

  _Module.Term();
#endif

  logging::CleanupChromeLogging();

#if defined(OS_MACOSX) && defined(GOOGLE_CHROME_BUILD)
  // TODO(mark): See the TODO(mark) above at InitCrashReporter.
  DestructCrashReporter();
#endif  // OS_MACOSX && GOOGLE_CHROME_BUILD

}

} // namespace Berkelium
