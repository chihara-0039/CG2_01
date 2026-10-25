#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ゲームのバックバッファだけを録画する。デスクトップ・マイクは取得しない。
// GPUコピーは既存のフレームフェンスで完了を確認し、圧縮は別スレッドへ渡す。
class FrameRecorder {
public:
    struct Options {
        int fps = 30;
        int outputWidth = 960;
        bool makeGif = true;
        int gifSeconds = 10;
        std::filesystem::path directory = "../generated/captures";
        std::string name; // 空なら日時を使う。比較撮影は方式名を指定できる。
    };
    ~FrameRecorder();
    bool Start(const Options& options);
    void Stop(); // UIを止めず、残りの圧縮とGIF変換をワーカーで終える。
    void Finish();
    void Prepare(ID3D12Device* device, ID3D12GraphicsCommandList* list, ID3D12Resource* target);
    void SubmitCompletedFrame(); // Prepareを含むGPU命令の完了後に限って呼ぶ。
    void DrawImGui();
    bool IsRecording() const { return recording_; }
    bool IsBusy() const { return busy_; }
    std::string Status() const;
    uint64_t Captured() const { return captured_; }
    uint64_t Dropped() const { return dropped_; }
private:
    struct Frame { uint64_t tick; std::vector<unsigned char> pixels; };
    void Encode();
    void SetStatus(const std::string& text);
    Options options_;
    std::filesystem::path ffmpeg_, mp4_, gif_, log_;
    std::atomic<bool> recording_{false}, busy_{false};
    std::atomic<uint64_t> captured_{0}, dropped_{0};
    std::atomic<uint64_t> readbackMicros_{0};
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Frame> queue_;
    std::thread worker_;
    std::string status_ = "Ready (F8: record / stop)";
    std::chrono::steady_clock::time_point started_;
    uint64_t lastTick_ = UINT64_MAX, pendingTick_ = 0;
    bool prepared_ = false;
    unsigned width_ = 0, height_ = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
    UINT64 bufferBytes_ = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> readback_;
};
