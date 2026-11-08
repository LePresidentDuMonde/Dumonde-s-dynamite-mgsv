#include "PartnerHorse.h"
#include "dynamite.h"
#include "windows.h"
#include <atomic>
#include <cstring>
#include <eh.h>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>

#include "Config.h"
#include "Messagebox.h"
#include "MinHook.h"
#include "Tpp/TppGameObject.h"
#include "Tpp/TppNPCLifeState.h"
#include "memtag.h"
#include "mgsvtpp_addresses_1_0_15_4_en.h"
#include "mgsvtpp_func_typedefs.h"
#include "patch.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include "util.h"

#include "DynamiteHook.h"
#include "DynamiteLua.h"

namespace Dynamite {
    void Dynamite::AbortHandler(int signal_number) {
        auto l = spdlog::get(logName);
        if (l != nullptr) {
            l->error("abort was called");
            l->flush();
        }
    }

    void TerminateHandler() {
        const auto l = spdlog::get(logName);
        if (l != nullptr) {
            l->error("terminate was called");
            l->flush();
        }

        terminate_Original();
    }

    // Logs where a crash happened. Runs before the game's own handler (which hangs instead of closing the game),
    // only for fatal kinds of exceptions, and never changes how they are handled. Offsets are exe addresses (base 0x140000000).
    LONG WINAPI Dynamite::UnhandledExceptionHandler(EXCEPTION_POINTERS *info) {
        const auto code = info->ExceptionRecord->ExceptionCode;
        if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_STACK_OVERFLOW &&
            code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_PRIV_INSTRUCTION) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        static std::atomic<int> logged = 0;
        const auto log = spdlog::get(logName);
        if (log == nullptr || logged++ >= 3) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        const auto exe = reinterpret_cast<uint64_t>(GetModuleHandle(nullptr));
        const auto where = [exe](uint64_t a) {
            HMODULE m = nullptr;
            if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(a), &m)) {
                return std::format("{:#x}", a);
            }
            if (reinterpret_cast<uint64_t>(m) == exe) {
                return std::format("exe {:#x}", a - exe + BaseAddr);
            }
            char name[MAX_PATH]{};
            GetModuleFileNameA(m, name, MAX_PATH);
            const auto slash = std::strrchr(name, '\\');
            return std::format("{}+{:#x}", slash ? slash + 1 : name, a - reinterpret_cast<uint64_t>(m));
        };

        const auto rec = info->ExceptionRecord;
        log->error("crash: code {:#x} at {}, access {} {:#x}", code, where(reinterpret_cast<uint64_t>(rec->ExceptionAddress)),
            rec->NumberParameters > 0 ? (rec->ExceptionInformation[0] == 1 ? "write" : "read") : "-", rec->NumberParameters > 1 ? rec->ExceptionInformation[1] : 0);

        // call chain from the faulting context
        CONTEXT ctx = *info->ContextRecord;
        for (int i = 0; i < 24 && ctx.Rip != 0; i++) {
            log->error("crash: frame {} {}", i, where(ctx.Rip));
            DWORD64 base = 0;
            const auto fn = RtlLookupFunctionEntry(ctx.Rip, &base, nullptr);
            if (fn == nullptr) {
                ctx.Rip = *reinterpret_cast<DWORD64 *>(ctx.Rsp);
                ctx.Rsp += 8;
                continue;
            }
            PVOID handlerData = nullptr;
            DWORD64 establisher = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, ctx.Rip, fn, &ctx, &handlerData, &establisher, nullptr);
        }
        log->flush();
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void Dynamite::ReadConfig() {
        ConfigValidateResult r{};
        if (!cfg.Read(&r)) {
            spdlog::error(r.Message);
            ShowMessageBox(r.Message.c_str(), "Config error", MB_ICONERROR);
            exit(1);
        }

        cfg.Log();
    }

    void Dynamite::RebaseAddresses() {
        for (const auto &[name, addr] : addressSet) {
            const uint64_t rebasedAddr = (addr - BaseAddr) + RealBaseAddr;
            addressSet[name] = rebasedAddr;
        }
    }

    void Dynamite::SetupLogger() {
        log = spdlog::basic_logger_st(logName, logPath.string(), true);
        spdlog::set_default_logger(log);
        // using `flush_every` with multithreaded logger will result in application hang during DLL_PROCESS_DETACH
        spdlog::flush_on(spdlog::level::info);
        log->info("starting");
        log->info("dynamite {}", SOFTWARE_VERSION);
    }

    Dynamite::Dynamite() : thisModule{GetModuleHandle(nullptr)} {
        signal(SIGABRT, &AbortHandler);
        terminate_Original = std::set_terminate(TerminateHandler);
        _set_abort_behavior(1, _WRITE_ABORT_MSG);
        AddVectoredExceptionHandler(1, UnhandledExceptionHandler);

        SetupLogger();

        // mgo bad
        HMODULE hExe = GetModuleHandle(nullptr);
        WCHAR fullPath[MAX_PATH]{0};
        GetModuleFileNameW(hExe, fullPath, MAX_PATH);
        std::filesystem::path path(fullPath);
        std::wstring exeName = path.filename().c_str();
        if (exeName.find(L"mgo") != std::wstring::npos) {
            return;
        }

        // addresses are valid only for one exe build, identified by PE timestamp
        const auto dos = (PIMAGE_DOS_HEADER)hExe;
        const auto nt = (PIMAGE_NT_HEADERS64)((uint8_t *)hExe + dos->e_lfanew);
        const auto timestamp = nt->FileHeader.TimeDateStamp;
        if (timestamp != exeTimestamp) {
            spdlog::error("unsupported mgsvtpp.exe build, PE timestamp {:#x}, expected {:#x} ({})", timestamp, exeTimestamp, exeVersion);
            const auto msg = std::format("This mgsvtpp.exe version is not supported by dynamite, mod is disabled.\n"
                                         "Supported: {}. See dynamite\\dynamite.log.txt",
                exeVersion);
            ShowMessageBox(msg.c_str(), "dynamite", MB_ICONWARNING);
            return;
        }

        MH_Initialize();

        addressSet = mgsvtppAddresses;
        RealBaseAddr = (size_t)GetModuleHandle(nullptr);

        RebaseAddresses();
        SetFuncPtrs();
        ReadConfig();
        CreateHooks();
        // PartnerHorse::Install(); disabled: forcing a second horse breaks co-op joining (2026-11-07), rework before enabling
        dynamiteCore.WithConfig(&cfg);
    }

    Dynamite::~Dynamite() {
        MH_Uninitialize();
    }


}