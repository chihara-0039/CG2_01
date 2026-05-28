#pragma once
#include <xaudio2.h>
#include <wrl.h>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")


class Sound
{
public:
    // 初期化
    void Initialize();

    // 終了処理
    void Finalize();

    // 音声データの構造体
    struct SoundData {
        // 波形フォーマット
        WAVEFORMATEX wfex;
        // バッファ
        std::vector<BYTE> buffer;
    };


    // wav / mp4 などをまとめて読み込む
    SoundData SoundLoadFile(const std::string& filename);

    // 音声データ解放
    void SoundUnload(SoundData* soundData);

    // 音声再生
    void SoundPlay(const SoundData& soundData, float volume = 1.0f);

    // BGM再生
    void BGMPlay(const SoundData& soundData, float volume = 0.5f);


    //音源停止
    void BGMStop();

private:
    // チャンクヘッダ
    struct ChunkHeader {
        char id[4];
        int32_t size;
    };

    // RIFFヘッダチャンク
    struct RiffHeader {
        ChunkHeader chunk;
        char type[4];
    };

    // FMTチャンク
    struct FormatChunk {
        ChunkHeader chunk;
        WAVEFORMATEX fmt;
    };

private:
    // 文字列変換
    std::wstring ConvertString(const std::string& str);

private:
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice = nullptr;

    // 読み込んだ音声データを保持しておく
    std::vector<SoundData> soundDatas;

    // 再生中のSourceVoiceを保持
    std::vector<IXAudio2SourceVoice*> sourceVoices;

    //現在再生中のBGM
    IXAudio2SourceVoice* bgmSourceVoice = nullptr;

};

