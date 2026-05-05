#include "Sound.h"
#include <cassert>
#include <cstring>
#include <Windows.h>
#include <combaseapi.h>

using namespace Microsoft::WRL;

std::wstring Sound::ConvertString(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (sizeNeeded == 0) {
        sizeNeeded = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
        assert(sizeNeeded != 0);

        std::wstring result(sizeNeeded - 1, L'\0');
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &result[0], sizeNeeded);
        return result;
    }

    std::wstring result(sizeNeeded - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], sizeNeeded);
    return result;
}

void Sound::Initialize() {
    HRESULT result;

    // Windows Media Foundationの初期化（ローカルファイル用途）
    result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    assert(SUCCEEDED(result));

    // XAudio2エンジンのインスタンスを生成
    result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(result));

    // マスターボイスを生成
    result = xAudio2->CreateMasteringVoice(&masterVoice);
    assert(SUCCEEDED(result));
}

void Sound::Finalize() {
    // SourceVoiceを先に停止・破棄
    for (IXAudio2SourceVoice* sourceVoice : sourceVoices) {
        if (sourceVoice) {
            sourceVoice->Stop();
            sourceVoice->FlushSourceBuffers();
            sourceVoice->DestroyVoice();
        }
    }
    sourceVoices.clear();

    // MasterVoiceを破棄
    if (masterVoice) {
        masterVoice->DestroyVoice();
        masterVoice = nullptr;
    }

    // XAudio2を解放
    xAudio2.Reset();

    // 音声データ解放
    for (SoundData& soundData : soundDatas) {
        SoundUnload(&soundData);
    }
    soundDatas.clear();

    HRESULT result = MFShutdown();
    assert(SUCCEEDED(result));
}


Sound::SoundData Sound::SoundLoadFile(const std::string& filename) {
    HRESULT result;

    // returnする為の音声データ
    SoundData soundData = {};

    // フルパスをワイド文字列に変換
    std::wstring filePathW = ConvertString(filename);

    // SourceReader作成
    ComPtr<IMFSourceReader> pReader;
    result = MFCreateSourceReaderFromURL(filePathW.c_str(), nullptr, &pReader);
    assert(SUCCEEDED(result));

    // PCM形式にフォーマット指定する
    ComPtr<IMFMediaType> pPCMType;
    result = MFCreateMediaType(&pPCMType);
    assert(SUCCEEDED(result));

    result = pPCMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    assert(SUCCEEDED(result));

    result = pPCMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    assert(SUCCEEDED(result));

    result = pReader->SetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        nullptr,
        pPCMType.Get()
    );
    assert(SUCCEEDED(result));

    // 実際にセットされたメディアタイプを取得
    ComPtr<IMFMediaType> pOutType;
    result = pReader->GetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        &pOutType
    );
    assert(SUCCEEDED(result));

    // Waveフォーマットを取得
    WAVEFORMATEX* waveFormat = nullptr;
    UINT32 waveFormatSize = 0;
    result = MFCreateWaveFormatExFromMFMediaType(
        pOutType.Get(),
        &waveFormat,
        &waveFormatSize
    );
    assert(SUCCEEDED(result));

    // SoundDataに格納
    soundData.wfex = *waveFormat;

    // 生成したWaveフォーマットを解放
    CoTaskMemFree(waveFormat);

    // PCMデータのバッファを構築
    while (true) {
        ComPtr<IMFSample> pSample;
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG llTimeStamp = 0;

        // サンプルを読み込む
        result = pReader->ReadSample(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            &streamIndex,
            &flags,
            &llTimeStamp,
            &pSample
        );
        assert(SUCCEEDED(result));

        // ストリームの末尾に達したら抜ける
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }

        // サンプルがない場合は次へ
        if (!pSample) {
            continue;
        }

        ComPtr<IMFMediaBuffer> pBuffer;

        // サンプルに含まれるサウンドデータのバッファを取得
        result = pSample->ConvertToContiguousBuffer(&pBuffer);
        assert(SUCCEEDED(result));

        BYTE* pData = nullptr;
        DWORD maxLength = 0;
        DWORD currentLength = 0;

        // バッファをロック
        result = pBuffer->Lock(&pData, &maxLength, &currentLength);
        assert(SUCCEEDED(result));

        // 読み込んだPCMデータを末尾に追加
        soundData.buffer.insert(
            soundData.buffer.end(),
            pData,
            pData + currentLength
        );

        // バッファをアンロック
        result = pBuffer->Unlock();
        assert(SUCCEEDED(result));
    }

    // 読み込んだ音声データを保持
    soundDatas.push_back(soundData);

    return soundData;
}

void Sound::SoundUnload(SoundData* soundData) {
    soundData->buffer.clear();
    soundData->wfex = {};
}

void Sound::SoundPlay(const SoundData& soundData, float volume) {
    HRESULT result;

    if (volume < 0.0f) {
        volume = 0.0f;
    }

    if (volume > 1.0f) {
        volume = 1.0f;
    }

    // 波形フォーマットを元にSourceVoiceの生成
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

    //音量設定
    result = pSourceVoice->SetVolume(volume);
    assert(SUCCEEDED(result));

    // 再生する波形データの設定
    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.buffer.data();
    buf.AudioBytes = static_cast<UINT32>(soundData.buffer.size());
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // 波形データの再生
    result = pSourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(result));

    result = pSourceVoice->Start();
    assert(SUCCEEDED(result));

    // ★終了時に破棄できるように保持
    sourceVoices.push_back(pSourceVoice);
}