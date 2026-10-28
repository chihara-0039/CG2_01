#include "FrameRecorder.h"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
std::wstring Quote(const std::filesystem::path& path) { return L"\"" + path.wstring() + L"\""; }
std::filesystem::path EncoderPath() {
    wchar_t value[32768]{};
    DWORD n = GetEnvironmentVariableW(L"CG2_FFMPEG", value, 32768);
    if (n && n < 32768 && std::filesystem::is_regular_file(value)) return value;
    if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, 32768, value, nullptr)) return value;
    // 開発PC向けの既定候補。配布PCではPATHまたはCG2_FFMPEGで明示する。
    const std::filesystem::path root = "../../Tools/FFmpeg";
    std::error_code error;
    if (std::filesystem::is_directory(root)) {
        for (const auto& item : std::filesystem::recursive_directory_iterator(root, error)) {
            if (item.path().filename() == "ffmpeg.exe") return std::filesystem::absolute(item.path());
        }
    }
    return {};
}
// シェルを介さずFFmpegのみを起動する。標準出力・エラーは録画ごとのログへ保存。
bool LaunchEncoder(const std::filesystem::path& exe, const std::wstring& arguments,
                   HANDLE input, HANDLE log, PROCESS_INFORMATION& process) {
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = startup.hStdError = log;
    std::wstring command = Quote(exe) + L" " + arguments;
    return CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                          CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE;
}
}

FrameRecorder::~FrameRecorder() { Finish(); }
void FrameRecorder::SetStatus(const std::string& text) { std::lock_guard lock(mutex_); status_ = text; }
std::string FrameRecorder::Status() const { std::lock_guard lock(mutex_); return status_; }
bool FrameRecorder::Start(const Options& options) {
    if (busy_) return false;
    if (worker_.joinable()) worker_.join();
    ffmpeg_ = EncoderPath();
    if (ffmpeg_.empty()) { SetStatus("FFmpeg missing: set CG2_FFMPEG to ffmpeg.exe"); return false; }
    options_ = options;
    options_.fps = std::clamp(options.fps, 10, 60);
    options_.outputWidth = std::clamp(options.outputWidth / 2 * 2, 320, 1920);
    options_.gifSeconds = std::clamp(options.gifSeconds, 1, 15);
    std::error_code error;
    std::filesystem::create_directories(options_.directory, error);
    if (error) { SetStatus("Cannot create capture directory"); return false; }
    std::string name = options_.name;
    if (name.empty()) {
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm local{}; localtime_s(&local, &now);
        std::ostringstream stream; stream << "capture_" << std::put_time(&local, "%Y%m%d_%H%M%S"); name = stream.str();
    }
    // 出力名からパス指定・引用符などを除き、既存の録画は上書きしない。
    for (char& c : name) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) c = '_';
    auto base = std::filesystem::absolute(options_.directory / name);
    for (unsigned suffix = 1; std::filesystem::exists(base.wstring() + L".mp4"); ++suffix)
        base = std::filesystem::absolute(options_.directory / (name + "_" + std::to_string(suffix)));
    mp4_ = base.wstring() + L".mp4"; gif_ = base.wstring() + L".gif"; log_ = base.wstring() + L".log";
    captured_ = 0; dropped_ = 0; readbackMicros_ = 0; lastTick_ = UINT64_MAX; prepared_ = false;
    width_ = height_ = 0; readback_.Reset();
    { std::lock_guard lock(mutex_); queue_.clear(); }
    started_ = std::chrono::steady_clock::now();
    recording_ = busy_ = true;
    SetStatus("Recording (silent). F8 stops; F9 shows controls.");
    worker_ = std::thread(&FrameRecorder::Encode, this);
    return true;
}
void FrameRecorder::Stop() { recording_ = false; ready_.notify_all(); }
void FrameRecorder::Finish() { Stop(); if (worker_.joinable()) worker_.join(); }

void FrameRecorder::Prepare(ID3D12Device* device, ID3D12GraphicsCommandList* list, ID3D12Resource* target) {
    prepared_ = false;
    if (!recording_) return;
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count();
    if (seconds >= 120.0) { Stop(); return; } // 意図しない長時間録画を防ぐ。
    const uint64_t tick = static_cast<uint64_t>(seconds * options_.fps);
    if (lastTick_ == tick) return;
    lastTick_ = tick;
    { std::lock_guard lock(mutex_); if (queue_.size() >= 8) { ++dropped_; return; } }
    const auto desc = target->GetDesc();
    if (!readback_) {
        if (desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
            SetStatus("Capture supports RGBA8 back buffers only"); Stop(); return;
        }
        width_ = static_cast<unsigned>(desc.Width); height_ = desc.Height;
        device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint_, nullptr, nullptr, &bufferBytes_);
        D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC buffer{}; buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = bufferBytes_; buffer.Height = 1; buffer.DepthOrArraySize = 1;
        buffer.MipLevels = 1; buffer.SampleDesc.Count = 1; buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback_)))) {
            SetStatus("Cannot allocate capture readback buffer"); Stop(); return;
        }
    }
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = target; barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1, &barrier);
    D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
    source.pResource = target; source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.pResource = readback_.Get(); destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint = footprint_;
    list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    list->ResourceBarrier(1, &barrier);
    pendingTick_ = tick; prepared_ = true;
}
void FrameRecorder::SubmitCompletedFrame() {
    if (!prepared_) return;
    prepared_ = false;
    if (!recording_) return;
    const auto cpuStarted=std::chrono::steady_clock::now();
    Frame frame; frame.tick = pendingTick_;
    frame.pixels.resize(static_cast<size_t>(width_) * height_ * 4);
    void* mapped = nullptr;
    D3D12_RANGE range{ 0, static_cast<SIZE_T>(bufferBytes_) };
    if (FAILED(readback_->Map(0, &range, &mapped))) { SetStatus("Readback map failed"); Stop(); return; }
    // DX12の256byteアラインメントを除去し、FFmpegへ密なRGBA列を渡す。
    for (unsigned y = 0; y < height_; ++y)
        std::memcpy(frame.pixels.data() + static_cast<size_t>(y) * width_ * 4,
                    static_cast<unsigned char*>(mapped) + footprint_.Offset + static_cast<size_t>(y) * footprint_.Footprint.RowPitch,
                    static_cast<size_t>(width_) * 4);
    D3D12_RANGE noWrite{0, 0}; readback_->Unmap(0, &noWrite);
    readbackMicros_ += static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-cpuStarted).count());
    { std::lock_guard lock(mutex_); queue_.push_back(std::move(frame)); ++captured_; }
    ready_.notify_one();
}
void FrameRecorder::Encode() {
    HANDLE readPipe = nullptr, writePipe = nullptr, logHandle = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION process{};
    SECURITY_ATTRIBUTES security{ sizeof(security), nullptr, TRUE };
    bool ok = true; uint64_t writtenFrames = 0, duplicated = 0;
    unsigned sourceWidth = 0, sourceHeight = 0;
    std::vector<unsigned char> previous;
    auto writePixels = [&](const std::vector<unsigned char>& bytes) {
        size_t offset = 0;
        while (offset < bytes.size()) {
            DWORD written = 0;
            if (!WriteFile(writePipe, bytes.data() + offset, static_cast<DWORD>(bytes.size() - offset), &written, nullptr) || !written) return false;
            offset += written;
        }
        ++writtenFrames; return true;
    };
    while (true) {
        Frame frame;
        {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [&] { return !queue_.empty() || !recording_; });
            if (queue_.empty()) break;
            frame = std::move(queue_.front()); queue_.pop_front();
            sourceWidth = width_; sourceHeight = height_;
        }
        if (!process.hProcess) {
            logHandle = CreateFileW(log_.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            ok = logHandle != INVALID_HANDLE_VALUE && CreatePipe(&readPipe, &writePipe, &security, 0);
            if (ok) ok = SetHandleInformation(writePipe, HANDLE_FLAG_INHERIT, 0) != FALSE;
            const auto arguments = L"-hide_banner -loglevel warning -n -f rawvideo -pixel_format rgba -video_size " +
                std::to_wstring(sourceWidth) + L"x" + std::to_wstring(sourceHeight) + L" -framerate " + std::to_wstring(options_.fps) +
                L" -i pipe:0 -an -vf scale=" + std::to_wstring(options_.outputWidth) + L":-2 -c:v libx264 -preset veryfast -crf 20 -pix_fmt yuv420p -movflags +faststart " + Quote(mp4_);
            if (ok) ok = LaunchEncoder(ffmpeg_, arguments, readPipe, logHandle, process);
            if (readPipe) { CloseHandle(readPipe); readPipe = nullptr; }
            if (!ok) break;
        }
        if (previous.empty()) previous = frame.pixels;
        // エンコーダーが遅れたときも時間を縮めず、抜けた時刻は直前の画像で補う。
        while (writtenFrames < frame.tick && ok) { ok = writePixels(previous); ++duplicated; }
        if (ok) ok = writePixels(frame.pixels);
        previous = std::move(frame.pixels);
        if (!ok) break;
    }
    recording_ = false;
    if (writePipe) CloseHandle(writePipe);
    if (readPipe) CloseHandle(readPipe);
    if (process.hProcess) {
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exit = 1; GetExitCodeProcess(process.hProcess, &exit); ok = ok && exit == 0;
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
    } else ok = false;
    if (ok && options_.makeGif) {
        SetStatus("MP4 saved; generating GIF...");
        PROCESS_INFORMATION conversion{};
        HANDLE nullInput = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
        const auto args = L"-hide_banner -loglevel warning -n -t " + std::to_wstring(options_.gifSeconds) + L" -i " + Quote(mp4_) +
            L" -filter_complex \"fps=12,scale=640:-1:flags=lanczos,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3\" -loop 0 " + Quote(gif_);
        bool gifOk = LaunchEncoder(ffmpeg_, args, nullInput, logHandle, conversion);
        if (gifOk) {
            WaitForSingleObject(conversion.hProcess, INFINITE);
            DWORD exit = 1; GetExitCodeProcess(conversion.hProcess, &exit); gifOk = exit == 0;
            CloseHandle(conversion.hThread); CloseHandle(conversion.hProcess);
        }
        if (nullInput != INVALID_HANDLE_VALUE) CloseHandle(nullInput);
        if (!gifOk) { SetStatus("MP4 saved, GIF failed: see capture log"); ok = false; }
    }
    if (logHandle != INVALID_HANDLE_VALUE) CloseHandle(logHandle);
    // 発表用の実測値。FPS向上など未測定の効果は記載しない。
    if (writtenFrames) {
        std::ofstream report(mp4_.wstring() + L".json");
        report << "{\"fps\":" << options_.fps << ",\"captured\":" << captured_.load()
            << ",\"queueDrops\":" << dropped_.load() << ",\"duplicateFrames\":" << duplicated
            << ",\"encodedFrames\":" << writtenFrames
            << ",\"meanReadbackCpuMs\":" << (captured_ ? double(readbackMicros_)/double(captured_)/1000.0 : 0.0)
            << ",\"audio\":false,\"success\":" << (ok ? "true" : "false") << "}";
    }
    if (ok) SetStatus("Saved: " + mp4_.filename().string());
    else if (Status().find("GIF failed") == std::string::npos) SetStatus("Recording failed: see capture .log (MP4 may be incomplete)");
    { std::lock_guard lock(mutex_); queue_.clear(); }
    busy_ = false;
}
void FrameRecorder::DrawImGui() {
#if defined(USE_IMGUI) && !defined(NDEBUG)
    static int fps = 30, width = 960;
    static bool gif = true;
    static char directory[512]="../generated/captures";
    ImGui::Begin("Engine Recorder (F8 / F9)");
    ImGui::TextWrapped("%s", Status().c_str());
    ImGui::Text("Captured: %llu / queue drops: %llu", static_cast<unsigned long long>(Captured()), static_cast<unsigned long long>(Dropped()));
    if (!IsBusy()) {
        ImGui::SliderInt("FPS", &fps, 10, 60);
        ImGui::SliderInt("Video width", &width, 320, 1920);
        ImGui::Checkbox("GIF (first 10 seconds, silent)", &gif);
        ImGui::InputText("Output folder (UTF-8)",directory,sizeof(directory));
        if (ImGui::Button("Start")) { Options options; options.fps = fps; options.outputWidth = width; options.makeGif = gif; options.directory=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(directory))); Start(options); }
    } else if (IsRecording() && ImGui::Button("Stop")) Stop();
    ImGui::TextWrapped("Output: ../generated/captures. F8 also works in Release. No audio capture.");
    ImGui::End();
#endif
}
