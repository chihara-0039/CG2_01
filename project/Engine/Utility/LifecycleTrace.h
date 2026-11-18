#pragma once
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace LifecycleTrace {
// 明示した計測実行だけに時刻を記録し、通常プレイではファイルを作らない。
inline void Mark(const char *phase) {
    struct Trace {
        std::ofstream output;
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        // 計測先が指定された時だけログを開く。
        Trace() {
            wchar_t path[32768]{};
            const auto size = GetEnvironmentVariableW(L"CG2_LIFECYCLE_LOG", path, 32768);
            if (size > 0 && size < 32768) {
                std::error_code error;
                std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
                output.open(std::filesystem::path(path));
                output << "phase,milliseconds\n";
            }
        }
    };
    static Trace trace;
    if (trace.output) {
        trace.output
            << phase << ','
            << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - trace.start).count()
            << '\n';
        trace.output.flush();
    }
}
}
