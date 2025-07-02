#define _USE_MATH_DEFINES
#include "vector"
#include <Windows.h>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <filesystem>
#include <math.h>
#include <string>
#include <numbers>
// #include <format>
#include <fstream>
#include <strsafe.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam);
#pragma region 構造体
struct Vector2 {
    float x, y;
};

struct Vector4 {
    float x, y, z, w;
};
struct Vector3 {
    float x, y, z;
};
struct Matrix4x4 {
    float m[4][4];
};
struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};
struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};
struct Fragment {
    Vector3 position;
    Vector3 velocity;
    Vector3 rotation;
    Vector3 rotationSpeed;
    float alpha;
    bool active;
};
struct Material {
    Vector4 color;
    int32_t enableLighting;
};
struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
};
struct DirectionalLight {
    Vector4 color;     // 光の色
    Vector3 direction; // 光の方向
    float intensity;   // 光の強さ
};

float Length(const Vector3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vector3 Normalize(const Vector3& v) {
    float len = Length(v);
    if (len == 0.0f) return { 0.0f, 0.0f, 0.0f };
    return { v.x / len, v.y / len, v.z / len };
}
#pragma endregion

#pragma region 変数
// 変数//--------------------
//
// 16分割
const int kSubdivision = 16;
// 頂点数
int kNumVertices = kSubdivision * kSubdivision * 6;
#pragma endregion

#pragma region 行列関数

//////////////---------------------------------------
// 関数の作成///
//////////////
//
// 単位行列の作成
Matrix4x4 MakeIdentity4x4() {
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i)
        result.m[i][i] = 1.0f;
    return result;
}
// 拡大縮小行列S
Matrix4x4 Matrix4x4MakeScaleMatrix(const Vector3& s) {
    Matrix4x4 result = {};
    result.m[0][0] = s.x;
    result.m[1][1] = s.y;
    result.m[2][2] = s.z;
    result.m[3][3] = 1.0f;
    return result;
}

// X軸回転行列R
Matrix4x4 MakeRotateXMatrix(float radian) {
    Matrix4x4 result = {};

    result.m[0][0] = 1.0f;
    result.m[1][1] = std::cos(radian);
    result.m[1][2] = std::sin(radian);
    result.m[2][1] = -std::sin(radian);
    result.m[2][2] = std::cos(radian);
    result.m[3][3] = 1.0f;

    return result;
}
// Y軸回転行列R
Matrix4x4 MakeRotateYMatrix(float radian) {
    Matrix4x4 result = {};

    result.m[0][0] = std::cos(radian);
    result.m[0][2] = std::sin(radian);
    result.m[1][1] = 1.0f;
    result.m[2][0] = -std::sin(radian);
    result.m[2][2] = std::cos(radian);
    result.m[3][3] = 1.0f;

    return result;
}
// Z軸回転行列R
Matrix4x4 MakeRotateZMatrix(float radian) {
    Matrix4x4 result = {};

    result.m[0][0] = std::cos(radian);
    result.m[0][1] = -std::sin(radian);
    result.m[1][0] = std::sin(radian);
    result.m[1][1] = std::cos(radian);
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;

    return result;
}

// 平行移動行列T
Matrix4x4 MakeTranslateMatrix(const Vector3& tlanslate) {
    Matrix4x4 result = {};
    result.m[0][0] = 1.0f;
    result.m[1][1] = 1.0f;
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;
    result.m[3][0] = tlanslate.x;
    result.m[3][1] = tlanslate.y;
    result.m[3][2] = tlanslate.z;

    return result;
}
// 行列の積
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                result.m[i][j] += m1.m[i][k] * m2.m[k][j];
    return result;
}
// ワールドマトリックス、メイクアフィン
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate,
    const Vector3& translate) {
    Matrix4x4 scaleMatrix = Matrix4x4MakeScaleMatrix(scale);
    Matrix4x4 rotateX = MakeRotateXMatrix(rotate.x);
    Matrix4x4 rotateY = MakeRotateYMatrix(rotate.y);
    Matrix4x4 rotateZ = MakeRotateZMatrix(rotate.z);
    Matrix4x4 rotateMatrix = Multiply(Multiply(rotateX, rotateY), rotateZ);
    Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

    Matrix4x4 worldMatrix =
        Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);
    return worldMatrix;
}
// 4x4 行列の逆行列を計算する関数
Matrix4x4 Inverse(Matrix4x4 m) {
    Matrix4x4 result;
    float det;
    int i;

    result.m[0][0] =
        m.m[1][1] * m.m[2][2] * m.m[3][3] - m.m[1][1] * m.m[2][3] * m.m[3][2] -
        m.m[2][1] * m.m[1][2] * m.m[3][3] + m.m[2][1] * m.m[1][3] * m.m[3][2] +
        m.m[3][1] * m.m[1][2] * m.m[2][3] - m.m[3][1] * m.m[1][3] * m.m[2][2];

    result.m[0][1] =
        -m.m[0][1] * m.m[2][2] * m.m[3][3] + m.m[0][1] * m.m[2][3] * m.m[3][2] +
        m.m[2][1] * m.m[0][2] * m.m[3][3] - m.m[2][1] * m.m[0][3] * m.m[3][2] -
        m.m[3][1] * m.m[0][2] * m.m[2][3] + m.m[3][1] * m.m[0][3] * m.m[2][2];

    result.m[0][2] =
        m.m[0][1] * m.m[1][2] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[3][2] -
        m.m[1][1] * m.m[0][2] * m.m[3][3] + m.m[1][1] * m.m[0][3] * m.m[3][2] +
        m.m[3][1] * m.m[0][2] * m.m[1][3] - m.m[3][1] * m.m[0][3] * m.m[1][2];

    result.m[0][3] =
        -m.m[0][1] * m.m[1][2] * m.m[2][3] + m.m[0][1] * m.m[1][3] * m.m[2][2] +
        m.m[1][1] * m.m[0][2] * m.m[2][3] - m.m[1][1] * m.m[0][3] * m.m[2][2] -
        m.m[2][1] * m.m[0][2] * m.m[1][3] + m.m[2][1] * m.m[0][3] * m.m[1][2];

    result.m[1][0] =
        -m.m[1][0] * m.m[2][2] * m.m[3][3] + m.m[1][0] * m.m[2][3] * m.m[3][2] +
        m.m[2][0] * m.m[1][2] * m.m[3][3] - m.m[2][0] * m.m[1][3] * m.m[3][2] -
        m.m[3][0] * m.m[1][2] * m.m[2][3] + m.m[3][0] * m.m[1][3] * m.m[2][2];

    result.m[1][1] =
        m.m[0][0] * m.m[2][2] * m.m[3][3] - m.m[0][0] * m.m[2][3] * m.m[3][2] -
        m.m[2][0] * m.m[0][2] * m.m[3][3] + m.m[2][0] * m.m[0][3] * m.m[3][2] +
        m.m[3][0] * m.m[0][2] * m.m[2][3] - m.m[3][0] * m.m[0][3] * m.m[2][2];

    result.m[1][2] =
        -m.m[0][0] * m.m[1][2] * m.m[3][3] + m.m[0][0] * m.m[1][3] * m.m[3][2] +
        m.m[1][0] * m.m[0][2] * m.m[3][3] - m.m[1][0] * m.m[0][3] * m.m[3][2] -
        m.m[3][0] * m.m[0][2] * m.m[1][3] + m.m[3][0] * m.m[0][3] * m.m[1][2];

    result.m[1][3] =
        m.m[0][0] * m.m[1][2] * m.m[2][3] - m.m[0][0] * m.m[1][3] * m.m[2][2] -
        m.m[1][0] * m.m[0][2] * m.m[2][3] + m.m[1][0] * m.m[0][3] * m.m[2][2] +
        m.m[2][0] * m.m[0][2] * m.m[1][3] - m.m[2][0] * m.m[0][3] * m.m[1][2];

    result.m[2][0] =
        m.m[1][0] * m.m[2][1] * m.m[3][3] - m.m[1][0] * m.m[2][3] * m.m[3][1] -
        m.m[2][0] * m.m[1][1] * m.m[3][3] + m.m[2][0] * m.m[1][3] * m.m[3][1] +
        m.m[3][0] * m.m[1][1] * m.m[2][3] - m.m[3][0] * m.m[1][3] * m.m[2][1];

    result.m[2][1] =
        -m.m[0][0] * m.m[2][1] * m.m[3][3] + m.m[0][0] * m.m[2][3] * m.m[3][1] +
        m.m[2][0] * m.m[0][1] * m.m[3][3] - m.m[2][0] * m.m[0][3] * m.m[3][1] -
        m.m[3][0] * m.m[0][1] * m.m[2][3] + m.m[3][0] * m.m[0][3] * m.m[2][1];

    result.m[2][2] =
        m.m[0][0] * m.m[1][1] * m.m[3][3] - m.m[0][0] * m.m[1][3] * m.m[3][1] -
        m.m[1][0] * m.m[0][1] * m.m[3][3] + m.m[1][0] * m.m[0][3] * m.m[3][1] +
        m.m[3][0] * m.m[0][1] * m.m[1][3] - m.m[3][0] * m.m[0][3] * m.m[1][1];

    result.m[2][3] =
        -m.m[0][0] * m.m[1][1] * m.m[2][3] + m.m[0][0] * m.m[1][3] * m.m[2][1] +
        m.m[1][0] * m.m[0][1] * m.m[2][3] - m.m[1][0] * m.m[0][3] * m.m[2][1] -
        m.m[2][0] * m.m[0][1] * m.m[1][3] + m.m[2][0] * m.m[0][3] * m.m[1][1];

    result.m[3][0] =
        -m.m[1][0] * m.m[2][1] * m.m[3][2] + m.m[1][0] * m.m[2][2] * m.m[3][1] +
        m.m[2][0] * m.m[1][1] * m.m[3][2] - m.m[2][0] * m.m[1][2] * m.m[3][1] -
        m.m[3][0] * m.m[1][1] * m.m[2][2] + m.m[3][0] * m.m[1][2] * m.m[2][1];

    result.m[3][1] =
        m.m[0][0] * m.m[2][1] * m.m[3][2] - m.m[0][0] * m.m[2][2] * m.m[3][1] -
        m.m[2][0] * m.m[0][1] * m.m[3][2] + m.m[2][0] * m.m[0][2] * m.m[3][1] +
        m.m[3][0] * m.m[0][1] * m.m[2][2] - m.m[3][0] * m.m[0][2] * m.m[2][1];

    result.m[3][2] =
        -m.m[0][0] * m.m[1][1] * m.m[3][2] + m.m[0][0] * m.m[1][2] * m.m[3][1] +
        m.m[1][0] * m.m[0][1] * m.m[3][2] - m.m[1][0] * m.m[0][2] * m.m[3][1] -
        m.m[3][0] * m.m[0][1] * m.m[1][2] + m.m[3][0] * m.m[0][2] * m.m[1][1];

    result.m[3][3] =
        m.m[0][0] * m.m[1][1] * m.m[2][2] - m.m[0][0] * m.m[1][2] * m.m[2][1] -
        m.m[1][0] * m.m[0][1] * m.m[2][2] + m.m[1][0] * m.m[0][2] * m.m[2][1] +
        m.m[2][0] * m.m[0][1] * m.m[1][2] - m.m[2][0] * m.m[0][2] * m.m[1][1];

    det = m.m[0][0] * result.m[0][0] + m.m[0][1] * result.m[1][0] +
        m.m[0][2] * result.m[2][0] + m.m[0][3] * result.m[3][0];

    if (det == 0)
        return Matrix4x4{}; // またはエラー処理

    det = 1.0f / det;

    for (i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            result.m[i][j] = result.m[i][j] * det;

    return result;
}
// 透視投影行列
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio,
    float nearClip, float farClip) {
    Matrix4x4 result = {};

    float f = 1.0f / std::tan(fovY / 2.0f);

    result.m[0][0] = f / aspectRatio;
    result.m[1][1] = f;
    result.m[2][2] = farClip / (farClip - nearClip);
    result.m[2][3] = 1.0f;
    result.m[3][2] = -(nearClip * farClip) / (farClip - nearClip);
    return result;
}
// 正射影行列
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right,
    float bottom, float nearClip, float farClip) {
    Matrix4x4 m = {};

    m.m[0][0] = 2.0f / (right - left);
    m.m[1][1] = 2.0f / (top - bottom);
    m.m[2][2] = 1.0f / (farClip - nearClip);
    m.m[3][0] = -(right + left) / (right - left);
    m.m[3][1] = -(top + bottom) / (top - bottom);
    m.m[3][2] = -nearClip / (farClip - nearClip);
    m.m[3][3] = 1.0f;

    return m;
}
// ビューポート変換行列
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height,
    float minDepth, float maxDepth) {
    Matrix4x4 m = {};

    // 行0：X方向スケーリングと移動
    m.m[0][0] = width / 2.0f;
    m.m[1][1] = -height / 2.0f;
    m.m[2][2] = maxDepth - minDepth;
    m.m[3][0] = left + width / 2.0f;
    m.m[3][1] = top + height / 2.0f;
    m.m[3][2] = minDepth;
    m.m[3][3] = 1.0f;

    return m;
}
#pragma endregion

#pragma region DX関数
// クラッシュ時のダンプ出力処理00_06
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
    // 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下ぶ出力
    SYSTEMTIME time;
    GetLocalTime(&time);
    wchar_t filePath[MAX_PATH] = { 0 };
    CreateDirectory(L"./Dumps", nullptr);
    StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
        time.wYear, time.wMonth, time.wDay, time.wHour,
        time.wMinute);
    HANDLE dumpFileHandle =
        CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    // processId(このexeのId)とクラッシュ(例外)の発生したthreadIdを取得
    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();
    // 設定情報を入力
    MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
    minidumpInformation.ThreadId = threadId;
    minidumpInformation.ExceptionPointers = exception;
    minidumpInformation.ClientPointers = TRUE;
    // Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
    MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle,
        MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
    // 他に関連づけられているSEH例外ハンドラがあれば実行。

    return EXCEPTION_EXECUTE_HANDLER;
}

void Log(std::ostream& os, const std::string& message) {
    os << message << std::endl;
    OutputDebugStringA(message.c_str());
}

std::wstring ConvertString(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }

    auto sizeNeeded =
        MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]),
            static_cast<int>(str.size()), NULL, 0);
    if (sizeNeeded == 0) {
        return std::wstring();
    }
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]),
        static_cast<int>(str.size()), &result[0], sizeNeeded);
    return result;
}

std::string ConvertString(const std::wstring& str) {
    if (str.empty()) {
        return std::string();
    }

    auto sizeNeeded =
        WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
            NULL, 0, NULL, NULL);
    if (sizeNeeded == 0) {
        return std::string();
    }
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
        result.data(), sizeNeeded, NULL, NULL);
    return result;
}

// 関数作成ヒープですか？ 02_03
ID3D12Resource* CreateBufferResource(ID3D12Device* device,
    size_t sizeInBytes) {

    // 頂点リソース用のヒープの設定02_03
    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // Uploadheapを使う
    // 頂点リソースの設定02_03
    D3D12_RESOURCE_DESC vertexResourceDesc{};
    // バッファリソース。テクスチャの場合はまた別の設定をする02_03
    vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vertexResourceDesc.Width = sizeInBytes; // リソースのサイズ　02_03
    // バッファの場合はこれらは１にする決まり02_03
    vertexResourceDesc.Height = 1;
    vertexResourceDesc.DepthOrArraySize = 1;
    vertexResourceDesc.MipLevels = 1;
    vertexResourceDesc.SampleDesc.Count = 1;
    // バッファの場合はこれにする決まり02_03
    vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // 実際に頂点リソースを作る02_03
    ID3D12Resource* vertexResource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&vertexResource));
    assert(SUCCEEDED(hr));

    return vertexResource;
}

// テクスチャデータを読む
ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device* device,
    D3D12_DESCRIPTOR_HEAP_TYPE heapType,
    UINT numDescriptors,
    bool shaderVisivle) {
    // ディスクリプタヒープの生成02_02
    ID3D12DescriptorHeap* DescriptorHeap = nullptr;

    D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc{};
    DescriptorHeapDesc.Type = heapType;
    DescriptorHeapDesc.NumDescriptors = numDescriptors;
    DescriptorHeapDesc.Flags = shaderVisivle
        ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = device->CreateDescriptorHeap(&DescriptorHeapDesc,
        IID_PPV_ARGS(&DescriptorHeap));
    // ディスクリプタヒープが作れなかったので起動できない
    assert(SUCCEEDED(hr)); // 1
    return DescriptorHeap;
}

// クリエイトテクスチャ03_00
ID3D12Resource* CreateTextureResource(ID3D12Device* device,
    const DirectX::TexMetadata& metadata) {
    // 1.metadataをもとにResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width);           // Textureの幅
    resourceDesc.Height = UINT(metadata.height);         // Textureの高さ
    resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipdmapの数
    resourceDesc.DepthOrArraySize =
        UINT16(metadata.arraySize);        // 奥行き　or 配列Textureの配列数
    resourceDesc.Format = metadata.format; // TextureのFormat
    resourceDesc.SampleDesc.Count = 1;     // サンプリングカウント。1固定
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(
        metadata.dimension); // Textureの次元数　普段使っているのは二次元
    // 2.利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM; // 細かい設定を行う
    heapProperties.CPUPageProperty =
        D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; // WriteBaackポリシーでCPUアクセス可能
    heapProperties.MemoryPoolPreference =
        D3D12_MEMORY_POOL_L0; // プロセッサの近くに配置

    // 3.Resourceを生成する
    ID3D12Resource* resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,                   // Heapの固定
        D3D12_HEAP_FLAG_NONE,              // Heapの特殊な設定。特になし
        &resourceDesc,                     // Resourceの設定
        D3D12_RESOURCE_STATE_GENERIC_READ, // 初回のResourceState.Textureは基本読むだけ
        nullptr,                           // Clear最適地。使わないのでnullptr
        IID_PPV_ARGS(&resource));          // 作成するResourceポインタへのポインタ
    assert(SUCCEEDED(hr));
    return resource;
}

// データを転送するUploadTextureData関数を作る03_00
void UploadTextureData(ID3D12Resource* texture,
    const DirectX::ScratchImage& mipImages) {
    // Meta情報を取得
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    // 全MipMapについて
    for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
        // MipMapLevelを指定して各Imageを取得
        const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
        // Textureに転送
        HRESULT hr =
            texture->WriteToSubresource(UINT(mipLevel), nullptr, img->pixels,
                UINT(img->rowPitch), UINT(img->slicePitch));
        assert(SUCCEEDED(hr));
    }
}

// ミップマップです03_00
DirectX::ScratchImage LoadTexture(const std::string& filePath) {
    // テクスチャファイルを読んでプログラムで扱えるようにする
    DirectX::ScratchImage image{};
    std::wstring filePathW = ConvertString(filePath);
    HRESULT hr = DirectX::LoadFromWICFile(
        filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    assert(SUCCEEDED(hr));

    // ミップマップの作成
    DirectX::ScratchImage mipImages{};
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
        image.GetMetadata(), DirectX::TEX_FILTER_SRGB,
        0, mipImages);
    assert(SUCCEEDED(hr));

    // ミップマップ付きのデータを返す
    return mipImages;
}
ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device,
    int32_t width,
    int32_t height) {

    // 生成するResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;        // Textureの幅
    resourceDesc.Height = height;      // Textureの高さ
    resourceDesc.MipLevels = 1;        // mipmapの数
    resourceDesc.DepthOrArraySize = 1; // 奥行き or 配列Textureの配列数
    resourceDesc.Format =
        DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
    resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1個
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
    resourceDesc.Flags =
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

    // 利用するHeapの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

    // 深度値のクリア設定
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.DepthStencil.Depth = 1.0f; // 最大値でクリア
    depthClearValue.Format =
        DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマットResourceと合わせる

    // Resourceの生成
    ID3D12Resource* resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,                  // Heapの設定
        D3D12_HEAP_FLAG_NONE,             // Heapの特殊な設定。特になし。
        &resourceDesc,                    // Resourceの設定
        D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
        &depthClearValue,                 // Clear最適値
        IID_PPV_ARGS(&resource));         // 作成するResourceポインタへのポインタ
    assert(SUCCEEDED(hr));
    return resource;
}
// CompileShader関数02_00
IDxcBlob* CompileShader(
    // CompilerするSHaderファイルへのパス02_00
    const std::wstring& filepath,
    // Compilerに使用するprofile02_00
    const wchar_t* profile,
    // 初期化で生成したものを3つ02_00
    IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler, std::ostream& os) {
    // ここの中身を書いていく02_00
    // 1.hlslファイルを読み込む02_00
    // これからシェーダーをコンパイルする旨をログに出す02_00
    Log(os, ConvertString(std::format(L"Begin CompileShader,path:{},profike:{}\n",
                                      filepath, profile)));
    // hlslファイルを読む02_00
    IDxcBlobEncoding* shaderSource = nullptr;
    HRESULT hr = dxcUtils->LoadFile(filepath.c_str(), nullptr, &shaderSource);
    // 読めなかったら止める02_00
    assert(SUCCEEDED(hr));
    // 読み込んだファイルの内容を設定する02_00
    DxcBuffer shaderSourceBuffer;
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding =
        DXC_CP_UTF8; // UTF8の文字コードであることを通知02_00
    // 2.Compileする
    LPCWSTR arguments[] = {
        filepath.c_str(), // コンパイル対象のhlslファイル名02_00
        L"-E",
        L"main", // エントリーポイントの指定。基本的にmain以外にはしない02_00
        L"-T",
        profile, // shaderProfileの設定02_00
        L"-Zi",
        L"-Qembed_debug", // デバック用の設定を埋め込む02_00
        L"-Od",           /// 最適化を外しておく02_00
        L"-Zpr"           // メモリレイアウトは行優先02_00

    };

    // 実際にShaderをコンパイルする02_00
    IDxcResult* shaderResult = nullptr;
    hr = dxcCompiler->Compile(

        &shaderSourceBuffer,        // 読み込んだファイル02_00
        arguments,                  // コンパイルオプション02_00
        _countof(arguments),        // コンパイルオプションの数02_00
        includeHandler,             // includeが含まれた諸々02_00
        IID_PPV_ARGS(&shaderResult) // コンパイル結果02_00
    );
    // コンパイルエラーではなくdxcが起動できないなど致命的な状況02_00
    assert(SUCCEEDED(hr));
    // 3.警告、エラーが出ていないか確認する02_00
    // 警告.エラーが出ていたらログに出して止める02_00
    IDxcBlobUtf8* shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        Log(os, shaderError->GetStringPointer());
        // 警告、エラーダメ絶対02_00
        assert(false);
    }
    // 4.Compile結果を受け取って返す02_00
    // コンパイル結果から実行用のバイナリ部分を取得02_00
    IDxcBlob* shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob),
        nullptr);
    assert(SUCCEEDED(hr));
    // 成功したログを出す02_00
    Log(os,
        ConvertString(std::format(L"Compile Succeeded, path:{}, profike:{}\n ",
                                  filepath, profile)));
    // もう使わないリソースを解放02_00
    shaderSource->Release();
    shaderResult->Release();
    // 実行用のバイナリを返却02_00
    return shaderBlob;
}

// 球の頂点生成関数
void GenerateSphereVertices(VertexData* vertices, int kSubdivision,
    float radius) {
    // 経度(360)
    const float kLonEvery = static_cast<float>(M_PI * 2.0f) / kSubdivision;
    // 緯度(180)
    const float kLatEvery = static_cast<float>(M_PI) / kSubdivision;

    for (int latIndex = 0; latIndex < kSubdivision; ++latIndex) {
        float lat = -static_cast<float>(M_PI) / 2.0f + kLatEvery * latIndex;

        for (int lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
            float lon = kLonEvery * lonIndex;

            // 初期位置
            uint32_t startIndex = (latIndex * kSubdivision + lonIndex) * 6;

            // 三角形1

            // a
            vertices[startIndex + 0].position.x = radius * cosf(lat) * cosf(lon);
            vertices[startIndex + 0].position.y = radius * sinf(lat);
            vertices[startIndex + 0].position.z = radius * cosf(lat) * sinf(lon);
            vertices[startIndex + 0].position.w = 1.0f;
            vertices[startIndex + 0].texcoord.x =
                static_cast<float>(lonIndex) / kSubdivision;
            vertices[startIndex + 0].texcoord.y =
                1.0f - static_cast<float>(latIndex) / kSubdivision;
            // b
            vertices[startIndex + 1].position.x =
                radius * cosf(lat + kLatEvery) * cosf(lon);
            vertices[startIndex + 1].position.y = radius * sinf(lat + kLatEvery);
            vertices[startIndex + 1].position.z =
                radius * cosf(lat + kLatEvery) * sinf(lon);
            vertices[startIndex + 1].position.w = 1.0f;
            vertices[startIndex + 1].texcoord.x =
                static_cast<float>(lonIndex) / kSubdivision;
            vertices[startIndex + 1].texcoord.y =
                1.0f - static_cast<float>(latIndex + 1) / kSubdivision;
            // c
            vertices[startIndex + 2].position.x =
                radius * cosf(lat) * cosf(lon + kLonEvery);
            vertices[startIndex + 2].position.y = radius * sinf(lat);
            vertices[startIndex + 2].position.z =
                radius * cosf(lat) * sinf(lon + kLonEvery);
            vertices[startIndex + 2].position.w = 1.0f;
            vertices[startIndex + 2].texcoord.x =
                static_cast<float>(lonIndex + 1) / kSubdivision;
            vertices[startIndex + 2].texcoord.y =
                1.0f - static_cast<float>(latIndex) / kSubdivision;

            // 三角形2:
            // c
            vertices[startIndex + 3].position.x = radius * cosf(lat) * cosf(lon + kLonEvery);
            vertices[startIndex + 3].position.y = radius * sinf(lat);
            vertices[startIndex + 3].position.z =
                radius * cosf(lat) * sinf(lon + kLonEvery);
            vertices[startIndex + 3].position.w = 1.0f;
            vertices[startIndex + 3].texcoord.x =
                static_cast<float>(lonIndex + 1) / kSubdivision;
            vertices[startIndex + 3].texcoord.y =
                1.0f - static_cast<float>(latIndex) / kSubdivision;
            // d
            vertices[startIndex + 5].position.x =
                radius * cosf(lat + kLatEvery) * cosf(lon + kLonEvery);
            vertices[startIndex + 5].position.y = radius * sinf(lat + kLatEvery);
            vertices[startIndex + 5].position.z =
                radius * cosf(lat + kLatEvery) * sinf(lon + kLonEvery);
            vertices[startIndex + 5].position.w = 1.0f;
            vertices[startIndex + 5].texcoord.x =
                static_cast<float>(lonIndex + 1) / kSubdivision;
            vertices[startIndex + 5].texcoord.y =
                1.0f - static_cast<float>(latIndex + 1) / kSubdivision;
            // b
            vertices[startIndex + 4].position.x =
                radius * cosf(lat + kLatEvery) * cosf(lon);
            vertices[startIndex + 4].position.y = radius * sinf(lat + kLatEvery);
            vertices[startIndex + 4].position.z =
                radius * cosf(lat + kLatEvery) * sinf(lon);
            vertices[startIndex + 4].position.w = 1.0f;
            vertices[startIndex + 4].texcoord.x =
                static_cast<float>(lonIndex) / kSubdivision;
            vertices[startIndex + 4].texcoord.y =
                1.0f - static_cast<float>(latIndex + 1) / kSubdivision;
        }
    }
}
// CG2_05_01_page_5
D3D12_CPU_DESCRIPTOR_HANDLE
GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap,
    uint32_t descriptorSize, uint32_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU =
        descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (descriptorSize * index);
    return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE
GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap,
    uint32_t descriptorSize, uint32_t index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU =
        descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += (descriptorSize * index);
    return handleGPU;
}
////////////////////
// 関数の生成ここまで//
////////////////////

#pragma endregion

    // 変数//
    // spriteトランスフォーム
Transform transformSprite{
    {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
// トランスフォーム
Transform transform{
    {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
// カメラトランスフォーム
Transform cameraTransform{
    {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -5.0f} };

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

    //
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    // メッセージに応じて固有の処理を行う
    switch (msg) {
        // ウィンドウが破棄された
    case WM_DESTROY:
    // OSに対して、アプリの終了を伝える
    PostQuitMessage(0);
    return 0;
    }
    // 標準のメッセージ処理を行う
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

////////////////
// main関数/////
///////////////

//  Windwsアプリでの円とリポウント(main関数)

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    CoInitializeEx(0, COINIT_MULTITHREADED);

    // 出力
    WNDCLASS wc{};
    // ウィンドウプロシージャ
    wc.lpfnWndProc = WindowProc;
    // ウィンドウクラス名(何でもよい)
    wc.lpszClassName = L"CG2WindowClass";
    // インスタンスバンドル
    wc.hInstance = GetModuleHandle(nullptr);
    // カーソル
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // ウィンドウクラスを登録する
    RegisterClass(&wc);
    // クライアント領域のサイズ
    const int32_t kClientWidth = 1280;
    const int32_t kClientHeight = 720;

    // ウィンドウサイズを表す構造体体にクライアント領域を入れる
    RECT wrc = { 0, 0, kClientWidth, kClientHeight };

    // クライアント領域をもとに実際のサイズにwrcを変更してもらう
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    // ウィンドウの生成
    HWND hwnd = CreateWindow(wc.lpszClassName, // 利用するクラス名
        L"CG2",           // タイトルバーの文字(何でもよい)
        WS_OVERLAPPEDWINDOW,  // よく見るウィンドウスタイル
        CW_USEDEFAULT,        // 表示X座標(Windowsに任せる)
        CW_USEDEFAULT,        // 表示Y座標(WindowsOSに任せる)
        wrc.right - wrc.left, // ウィンドウ横幅
        wrc.bottom - wrc.top, // ウィンドウ縦幅
        nullptr,              // 親ウィンドウハンドル
        nullptr,              // メニューハンドル
        wc.hInstance,         // インスタンスハンドル
        nullptr);             // オプション

    // 01_01
#ifdef _DEBUG

    ID3D12Debug1* debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        // デバックレイヤーを有効化する
        debugController->EnableDebugLayer();
        // さらに6PU側でもチェックリストを行うようにする
        debugController->SetEnableGPUBasedValidation(TRUE);
    }

#endif // _DEBUG

    // ウィンドウを表示する
    ShowWindow(hwnd, SW_SHOW);

#pragma region 起動時ログ設定とクラッシュ対策 00_04,00_06
    // 誰も補足しなかった場合(Unhandled),補足する関数を登録
    // main関数はじまってすぐに登録するとよい
    SetUnhandledExceptionFilter(ExportDump); // 00_06の関数ダンプ関数
    // ログのディレクトリを用意
    std::filesystem::create_directory("logs");
    // main関数の先頭02_04

    // 現在時刻を取得(UTC時刻)
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    // ログファイルの名前にコンマ何秒はいらないので削って秒にする
    std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
        nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    // 日本時間(PCの設定時間)に変換
    std::chrono::zoned_time loacalTime{ std::chrono::current_zone(), nowSeconds };
    // formatを使って年月日_時分秒の文字列に変換
    std::string dateString = std::format("{:%Y%m%d_%H%M%S}", loacalTime);
    // 時刻を使ってファイル名を決定
    std::string logFilePath = std::string("logs/") + dateString + ".log";
    // ファイルを作って書き込み準備
    std::ofstream logStream(logFilePath);

#pragma endregion

#pragma region DirectX12の初期化（アダプターとデバイス作成）00_05

    // DXGIファクトリーの生成00_05
    IDXGIFactory7* dxgiFactory = nullptr;
    // HRESULTはWindows系のエラー子どであり
    // 関数が成功したか銅貨をSUCCEEDEDマクロで判定できる
    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
    // 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、どうにもできない場合が多いのでassertにしておく
    assert(SUCCEEDED(hr));
    // 説明
    // DXGIファクトリーってのは、「PCにあるグラフィック機能を調べるための係」みたいなもの。
    // CreateDXGIFactory は、その係を呼び出してる。
    // うまく呼び出せなかったら assert() でプログラムを止める。

    //===============================================//

    // どのグラフィックカードを使うか調べる
    //  使用するアダプタ用の変数,最初にnullptrを入れておく
    IDXGIAdapter4* useAdapter = nullptr;
    // よい順にアダプタを頼む
    for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
        i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
        ++i) {
        // アダプターの情報を取得する
        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr)); // 取得できないのは一大事
        // ソフトウェアアダプタでなければ採用!
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            // 採用したアダプタの情報をログに出力wstringの方なので注意
            Log(logStream, ConvertString(std::format(L"Use Adapater:{}\n",
                                                     adapterDesc.Description)));
            break;
        }
        useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
    }
    // 適切なアダプタが見つからなかったので起動できない
    assert(useAdapter != nullptr);
    // 説明
    // PCの中にある**グラフィックカード（GPU）**を順番にチェック。
    // グラボを見つけたら、それを使うやつに決定！
    // なかったら、止める（＝DirectXは起動できない）

    //===============================================//

    // DirectXのデバイスを作る
    ID3D12Device* device = nullptr;
    // 昨日レベルとログ出力用の文字列
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0 };

    const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };
    // 高い順に生成できるか試していく
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        // 採用したアダプターでデバイスを生成
        hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
        // 指定した昨日レベルでデバイスは生成できたか確認
        if (SUCCEEDED(hr)) {
            // 生成できたのでログ出力を行ってループを抜ける
            Log(logStream,
                std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
            break;
        }
    }
    // デバイスの生成が上手くいかなかったので起動できない
    assert(device != nullptr);
    Log(logStream,
        "Complete create D3D12Device!!!\n"); // 初期化完了のログを出す
    // 説明
    // 「このグラボで、DirectX 12 のどのバージョンが使えるか？」をチェック。
    // 一番強いバージョンから順に試す。
    // 成功したらそのバージョンでデバイスDirectX本体を作る。
    // 最後に「デバイス作れたよ！」ってログに出す。

#pragma endregion

// 01_01
#ifdef _DEBUG
    ID3D12InfoQueue* infoQueue = nullptr;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        // やばいエラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        // エラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        // 警告時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        // 抑制するメッセージのＩＤ
        D3D12_MESSAGE_ID denyIds[] = {
            // windows11でのDXGIデバックレイヤーとDX12デバックレイヤーの相互作用バグによるエラーメッセージ
            // https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE };
        // 抑制するレベル
        D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        // 指定したメッセージの表示wp抑制する
        infoQueue->PushStorageFilter(&filter);
        // 解放
        infoQueue->Release();
    }

#endif // DEBUG

#pragma region コマンドキュー・アロケーター・リスト初期化01_00
    // コマンドキューを生成する
    ID3D12CommandQueue* commandQueue = nullptr;
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    hr = device->CreateCommandQueue(&commandQueueDesc,
        IID_PPV_ARGS(&commandQueue));
    // 説明
    //  GPU への命令を順番に並べて送る「列（キュー）」です。
    //  コマンドキューの生成が上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));

    // コマンドアロケーターを生成する
    ID3D12CommandAllocator* commandAllocator = nullptr;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator));
    // コマンドキューアロケーターの生成があ上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));
    // 説明
    // 命令（コマンド）を ためておくメモリ領域。

    // コマンドリストを生成する
    ID3D12GraphicsCommandList* commandList = nullptr;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator, nullptr,
        IID_PPV_ARGS(&commandList));
    // コマンドリストの生成が上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));
    // 説明
    // ここに描画命令を書き、最後にキューに送ります。
#pragma endregion

#pragma region スワップチェーンの生成処理01_00
  // スワップチェーンを生成する
    IDXGISwapChain4* swapChain = nullptr;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width =
        kClientWidth; // 画面の幅。ウィンドウのクライアント領域を同じものんにしておく
    swapChainDesc.Height =
        kClientHeight; // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
    swapChainDesc.SampleDesc.Count = 1;                // マルチサンプルしない
    swapChainDesc.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとしてりようする
    swapChainDesc.BufferCount = 2;       // ダブルバッファ
    swapChainDesc.SwapEffect =
        DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニターに移したら,中身を吐き
    // コマンドキュー,ウィンドウバンドル、設定を渡して生成する
    hr = dxgiFactory->CreateSwapChainForHwnd(
        commandQueue, hwnd, &swapChainDesc, nullptr, nullptr,
        reinterpret_cast<IDXGISwapChain1**>(&swapChain));
    assert(SUCCEEDED(hr));
    // 説明
    // スワップチェーンは「絵を描くキャンバスを2枚用意して、1枚に描いて、もう1枚をお客さんに見せる」仕組みです。
    // これで「描いてる途中の絵」を見せないようにできるんだよ。
#pragma endregion

#pragma region RTVヒープ作成とスワップチェーンのRTV設定 01_00

  // RTV用のヒープでディスクリプタの数は２。RTVはSHADER内で触るものではないので、shaderVisivleはfalse02_02
    ID3D12DescriptorHeap* rtvDescriptorHeap =
        CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

    ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap(
        device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

    // SwapChainからResourceを引っ張ってくる
    ID3D12Resource* swapChainResources[2] = { nullptr };
    hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
    // 上手く取得できなければ起動できない
    assert(SUCCEEDED(hr));
    hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
    assert(SUCCEEDED(hr));

    // RTVの設定
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    // ディスクリプタの先頭を取得する
    D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle =
        rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    // RTVを2つ作るのでディスクリプタを２つ用意
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
    // まず１つ目をつくる。１つ目は最初のところに作る。作る場所をこちらで指定して上げる必要がある
    rtvHandles[0] = rtvStartHandle;
    device->CreateRenderTargetView(swapChainResources[0], &rtvDesc,
        rtvHandles[0]);

    // 2つ目のディスクリプタハンドルを得る（自力で）
    rtvHandles[1].ptr =
        rtvHandles[0].ptr +
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    // 2つ目を作る
    device->CreateRenderTargetView(swapChainResources[1], &rtvDesc,
        rtvHandles[1]);
    // 説明
    // 画面に描くためのレンダーターゲットビューを2つ用意してる（ダブルバッファ用）。
    // スワップチェーンのバッファをRTVでGPUが描画可能な形にしてヒープに登録している。
    // これによって「どのバッファに描くか」をGPUに教えられるようになる。
#pragma endregion

#pragma region ディスクリプタサイズを取得する（SRV/RTV/DSV）
  // DescriptorSizeを取得しておくCG2_05_01_page_6
    const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const uint32_t descriptorSizeRTV =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const uint32_t descriptorSizeDSV =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
#pragma endregion

    // 03_01の読み書きのやつ
    ID3D12Resource* depthStencilResource =
        CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);
    // 03_01
    ID3D12DescriptorHeap* dsvDescriptorHeap =
        CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    // DSVの設定03_01
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;        // Resourceと合わせる
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2D texture

    // DSVを作成03_01
    device->CreateDepthStencilView(
        depthStencilResource, &dsvDesc,
        dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // 初期値でFenceを作る01_02
    ID3D12Fence* fence = nullptr;
    uint64_t fenceValue = 0;
    hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence));
    assert(SUCCEEDED(hr));

    // FenceのSignalを待つためのイベントを作成する01_02
    HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent != nullptr);

    // dxcCompilerを初期化CG2_02_00
    IDxcUtils* dxcUtils = nullptr;
    IDxcCompiler3* dxcCompiler = nullptr;
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
    assert(SUCCEEDED(hr));

    // 現時点でincludeはしないがincludeに対応するための設定を行っておく
    IDxcIncludeHandler* includHandler = nullptr;
    hr = dxcUtils->CreateDefaultIncludeHandler(&includHandler);
    assert(SUCCEEDED(hr));
    // ==== ルートシグネチャを作る準備 ====
    // RootSignature作成02_00
    // 頂点データの形式を使っていいよ！というフラグを立てる
    // ルート何？03_00
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter =
        D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
    staticSamplers[0].AddressU =
        D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0〜1の範囲外をリピート
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipMapを使う
    staticSamplers[0].ShaderRegister = 0;         // レジスタ番号0を使う
    staticSamplers[0].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う

    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);
    // RootParameter作成。複数設定できるので配列。今回は結果１つだけなので長さ１の配列
    // PixelShaderのMaterialとVertexShaderのTransform
    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
    rootParameters[0].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;               // PixelShaderで使う
    rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号０とバインド
    // ここから[2]
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
    rootParameters[1].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;              // Vertexshaderで使う
    rootParameters[1].Descriptor.ShaderRegister = 0; // 得wジスタ番号０を使う


    // ここまで[2]
    // 新しいディスクリプタレンジ03_00
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0;                      // 0から始まる
    descriptorRange[0].NumDescriptors = 1;                          // 数は1つ
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
    descriptorRange[0].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算
    descriptionRootSignature.pParameters =
        rootParameters; // ルートパラメータ配列へのポインタ
    descriptionRootSignature.NumParameters =
        _countof(rootParameters); // 配列の長さ

    // 新しいディスクリプタレンジ03_00
    // ここから[3]03_00
    rootParameters[2].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
    rootParameters[2].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
    rootParameters[2].DescriptorTable.pDescriptorRanges =
        descriptorRange; // Tableの中身の配列を指定
    rootParameters[2].DescriptorTable.NumDescriptorRanges =
        _countof(descriptorRange); // Tableで利用する数
    // ここまで[3]
    rootParameters[3].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_CBV; // DescriptorTableを使う
    rootParameters[3].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
    rootParameters[3].Descriptor.ShaderRegister = 1;

    // ==== シリアライズしてバイナリにする（GPUが読める形に変換） ====
    // バイナリになるデータを入れるための箱02_00
    ID3DBlob* signatureBlob = nullptr; // ルートシグネチャ本体
    ID3DBlob* errorBlob = nullptr;     // エラー内容が入るかも
    // GPUが読めるようにデータ変換！（バイナリ化）
    hr = D3D12SerializeRootSignature(&descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob,
        &errorBlob);

    // もし失敗したら、エラーメッセージを出して止める
    if (FAILED(hr)) {
        Log(logStream, reinterpret_cast<char*>(
            errorBlob->GetBufferPointer())); // エラーをログに出す
        assert(false); // 絶対成功してないと困るので、止める
    }

    // バイナリをもとに生成02_00
    ID3D12RootSignature* rootSignature = nullptr;
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature));
    assert(SUCCEEDED(hr));

    // Textureを読んで転送する03_00
    DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    ID3D12Resource* textureResource = CreateTextureResource(device, metadata);
    UploadTextureData(textureResource, mipImages);

    // 2枚目のTextureを読んで転送するCG2_05_01_page_8
    DirectX::ScratchImage mipImages2 = LoadTexture("resources/monsterBall.png");
    const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
    ID3D12Resource* textureResource2 = CreateTextureResource(device, metadata2);
    UploadTextureData(textureResource2, mipImages2);

    // === SRV共通の設定 ===
    UINT incrementSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // metaDataを基にSRVの設定03_00
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

    // metaData2を基にSRVの設定CG2_05_01_page_9
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
    srvDesc2.Format = metadata2.format;
    srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
    srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

    // SRVを作成するDescriptorHeapの場所を決める//変更CG2_05_01_0page6
    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU =
        GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU =
        GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);

    // SRVを作成するDescriptorHeapの場所を決めるCG2_05_01_page_9
    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 =
        GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 =
        GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);

    // 05_01_page6これ消せ
    //// 先頭はImGuiが使っているのでその次を使う
    // textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(
    //     D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    // textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(
    //     D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // SRVの生成03_00
    device->CreateShaderResourceView(textureResource, &srvDesc,
        textureSrvHandleCPU);
    device->CreateShaderResourceView(textureResource2, &srvDesc2,
        textureSrvHandleCPU2);
    // SRVの生成CG2_05_01_page_9
    //  InputLayout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // BlendStateの設定
    D3D12_BLEND_DESC blendDesc{};
    // 全ての色要素を書き込む
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    // RasiterzerStateの設定
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    // 裏面(時計回り)を表示しない
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    // 三角形の中を塗りつぶす
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // Shaderをコンパイルする
    IDxcBlob* vertexShaderBlob =
        CompileShader(L"Object3d.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler,
            includHandler, logStream);
    assert(vertexShaderBlob != nullptr);

    IDxcBlob* pixelShaderBlob =
        CompileShader(L"Object3d.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler,
            includHandler, logStream);
    assert(pixelShaderBlob != nullptr);

    // PSOを生成する
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature; // RootSignatrue
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;  // InputLayout
    graphicsPipelineStateDesc.VS = {
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize() }; // VertexShader
    graphicsPipelineStateDesc.PS = {
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize() };                      // PixelShader
    graphicsPipelineStateDesc.BlendState = blendDesc;           // BlensState
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
    // DepthStencilStateの設定03_01
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    // Depth の機能を有効化する03_01
    depthStencilDesc.DepthEnable = true;
    // 書き込みします03_01
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    // 比較関数は LessEqual。つまり、近ければ描画される03_01
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    // 書き込むRTVの情報03_01
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    // 03_01

    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    // 利用するトポロジ(形状)のタイプ。三角形
    graphicsPipelineStateDesc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // どのように画面に色を打ち込むかの設定(気にしなくて良い)
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    // 実際に生成
    ID3D12PipelineState* graphicsPinelineState = nullptr;
    hr = device->CreateGraphicsPipelineState(
        &graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPinelineState));
    assert(SUCCEEDED(hr));

    ID3D12Resource* vertexResource =
        CreateBufferResource(device, sizeof(VertexData) * kNumVertices);

    // sprite用の頂点リソースを作る04_00
    ID3D12Resource* vertexResourceSprite =
        CreateBufferResource(device, sizeof(VertexData) * kNumVertices);

    ID3D12Resource* materialResourceSprite = CreateBufferResource(device, sizeof(Material));

    Material* materialDataSprite = nullptr;

    materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));

    materialDataSprite->color = Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };


    materialDataSprite->enableLighting = false;

    // 頂点バッファビューを作成する
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    // リソースの先頭のアドレスから使う
    vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
    // 使用するリソースのサイズは頂点３つ分のサイズ
    vertexBufferView.SizeInBytes = sizeof(VertexData) * kNumVertices;
    // 1頂点あたりのサイズ
    vertexBufferView.StrideInBytes = sizeof(VertexData);

    // sprite用の頂点バッファビューを作成する04_00
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
    // sprite用のリソースの先頭のアドレスから使う04_00
    vertexBufferViewSprite.BufferLocation =
        vertexResourceSprite->GetGPUVirtualAddress();
    // sprite用の使用するリーソースのサイズは頂点6つ分のサイズ04_00
    vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * kNumVertices;
    // sprite用の１頂点当たりのサイズ04_00
    vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

    // 頂点リソースにデータを書き込む
    VertexData* vertexData = nullptr;
    // sprite用の頂点リソースにデータを書き込む04_00
    VertexData* vertexDataSprite = nullptr;
    //  書き込むためのアドレスを取得----------------------03_00
    vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));


    //// 左下
    //vertexData[0].position = { -0.5f, -0.5f, 0.0f, 1.0f };
    //vertexData[0].texcoord = { 0.0f, 1.0f };
    //vertexData[0].normal = { 0.0f, 0.0f ,1.0f };
    ////  上
    //vertexData[1].position = { 0.0f, 0.5f, 0.0f, 1.0f };
    //vertexData[1].texcoord = { 0.5f, 0.0f };
    //vertexData[1].normal = { 0.0f, 0.0f ,1.0f };
    ////  右下
    //vertexData[2].position = { 0.5f, -0.5f, 0.0f, 1.0f };
    //vertexData[2].texcoord = { 1.0f, 1.0f };
    //vertexData[2].normal = { 0.0f, 0.0f ,1.0f };
    //// 左下２
    //vertexData[3].position = { -0.5f, -0.5f, 0.5f, 1.0f };
    //vertexData[3].texcoord = { 0.0f, 1.0f };
    //vertexData[3].normal = { 0.0f, 0.0f ,1.0f };
    //// 上２
    //vertexData[4].position = { 0.0f, 0.0f, 0.0f, 1.0f };
    //vertexData[4].texcoord = { 0.5f, 0.0f };
    //vertexData[4].normal = { 0.0f, 0.0f ,1.0f };
    //// 右下
    //vertexData[5].position = { 0.5f, -0.5f, -0.5f, 1.0f };
    //vertexData[5].texcoord = { 1.0f, 1.0f };
    //vertexData[5].normal = { 0.0f, 0.0f ,1.0f };

    /*--- 球体 ---*/

//分裂数
    const uint32_t kSubdivision = 16;

    //経度分割1つ分の角度
    const float kLonEvery = 2.0f * std::numbers::pi_v<float> / float(kSubdivision);

    //緯度分割1つ分の角度
    const float kLatEvery = std::numbers::pi_v<float> / float(kSubdivision);

    //必要な頂点数
    const uint32_t vertexCount = kSubdivision * kSubdivision * 6;



    //緯度の方向に分割 -π/2 ~ π/2
    for (uint32_t latIndex = 0; latIndex < kSubdivision; latIndex++) {
        float lat = -std::numbers::pi_v<float> / 2.0f + kLatEvery * latIndex;

        //経度の方向に分割 0 ~ 2π
        for (uint32_t lonIndex = 0; lonIndex < kSubdivision; lonIndex++) {
            uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;

            //現在の経度
            float lon = lonIndex * kLonEvery;

            //頂点にデータを入力する

            //三角形1つ目: a,b,c

            //a
            vertexData[start + 0].position.x = cos(lat) * cos(lon);
            vertexData[start + 0].position.y = sin(lat);
            vertexData[start + 0].position.z = cos(lat) * sin(lon);
            vertexData[start + 0].position.w = 1.0f;

            vertexData[start + 0].texcoord = {
                float(lonIndex) / float(kSubdivision),
                 1.0f - float(latIndex) / float(kSubdivision)
            };

            vertexData[start + 0].normal.x = vertexData[start + 0].position.x;
            vertexData[start + 0].normal.y = vertexData[start + 0].position.y;
            vertexData[start + 0].normal.z = vertexData[start + 0].position.z;


            //b
            vertexData[start + 1].position.x = cos(lat + kLatEvery) * cos(lon);
            vertexData[start + 1].position.y = sin(lat + kLatEvery);
            vertexData[start + 1].position.z = cos(lat + kLatEvery) * sin(lon);
            vertexData[start + 1].position.w = 1.0f;

            vertexData[start + 1].texcoord = {
                float(lonIndex) / float(kSubdivision),
                 1.0f - float(latIndex + 1) / float(kSubdivision)
            };

            vertexData[start + 1].normal.x = vertexData[start + 1].position.x;
            vertexData[start + 1].normal.y = vertexData[start + 1].position.y;
            vertexData[start + 1].normal.z = vertexData[start + 1].position.z;

            //c
            vertexData[start + 2].position.x = cos(lat) * cos(lon + kLonEvery);
            vertexData[start + 2].position.y = sin(lat);
            vertexData[start + 2].position.z = cos(lat) * sin(lon + kLonEvery);
            vertexData[start + 2].position.w = 1.0f;

            vertexData[start + 2].texcoord = {
                float(lonIndex + 1) / float(kSubdivision),
                 1.0f - float(latIndex) / float(kSubdivision)
            };

            vertexData[start + 2].normal.x = vertexData[start + 2].position.x;
            vertexData[start + 2].normal.y = vertexData[start + 2].position.y;
            vertexData[start + 2].normal.z = vertexData[start + 2].position.z;

            //三角形2つ目: c,b,d

            // c
            vertexData[start + 3].position.x = vertexData[start + 2].position.x;
            vertexData[start + 3].position.y = vertexData[start + 2].position.y;
            vertexData[start + 3].position.z = vertexData[start + 2].position.z;
            vertexData[start + 3].position.w = vertexData[start + 2].position.w;

            vertexData[start + 3].texcoord = vertexData[start + 2].texcoord;

            vertexData[start + 3].normal.x = vertexData[start + 3].position.x;
            vertexData[start + 3].normal.y = vertexData[start + 3].position.y;
            vertexData[start + 3].normal.z = vertexData[start + 3].position.z;


            // b
            vertexData[start + 4].position.x = vertexData[start + 1].position.x;
            vertexData[start + 4].position.y = vertexData[start + 1].position.y;
            vertexData[start + 4].position.z = vertexData[start + 1].position.z;
            vertexData[start + 4].position.w = vertexData[start + 1].position.w;

            vertexData[start + 4].texcoord = vertexData[start + 1].texcoord;

            vertexData[start + 4].normal.x = vertexData[start + 4].position.x;
            vertexData[start + 4].normal.y = vertexData[start + 4].position.y;
            vertexData[start + 4].normal.z = vertexData[start + 4].position.z;


            //d
            vertexData[start + 5].position.x = cos(lat + kLatEvery) * cos(lon + kLonEvery);
            vertexData[start + 5].position.y = sin(lat + kLatEvery);
            vertexData[start + 5].position.z = cos(lat + kLatEvery) * sin(lon + kLonEvery);
            vertexData[start + 5].position.w = 1.0f;

            vertexData[start + 5].texcoord = {
                float(lonIndex + 1) / float(kSubdivision),
                 1.0f - float(latIndex + 1) / float(kSubdivision)
            };

            vertexData[start + 5].normal.x = vertexData[start + 5].position.x;
            vertexData[start + 5].normal.y = vertexData[start + 5].position.y;
            vertexData[start + 5].normal.z = vertexData[start + 5].position.z;

        }
    }

    //  書き込むためのアドレスを取得----------------------04_00
    vertexResourceSprite->Map(
        0, nullptr,
        reinterpret_cast<void**>(&vertexDataSprite)); // 04_00
    // 1枚目の三角形
    vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f }; // 左下04_00
    vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
    vertexDataSprite[0].normal = { 0.0f, 0.0f ,1.0f };
    vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上04_00
    vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
    vertexDataSprite[1].normal = { 0.0f, 0.0f ,1.0f };
    vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f }; // 右下04_00
    vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
    vertexDataSprite[2].normal = { 0.0f, 0.0f ,1.0f };
    // ２枚目の三角形
    vertexDataSprite[3].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左下04_00
    vertexDataSprite[3].texcoord = { 0.0f, 0.0f };
    vertexDataSprite[3].normal = { 0.0f, 0.0f ,1.0f };
    vertexDataSprite[4].position = { 640.0f, 0.0f, 0.0f, 1.0f }; // 左上04_00
    vertexDataSprite[4].texcoord = { 1.0f, 0.0f };
    vertexDataSprite[4].normal = { 0.0f, 0.0f ,1.0f };
    vertexDataSprite[5].position = { 640.0f, 360.0f, 0.0f, 1.0f }; // 右下04_00
    vertexDataSprite[5].texcoord = { 1.0f, 1.0f };
    vertexDataSprite[5].normal = { 0.0f, 0.0f ,1.0f };
    // スフィア
    GenerateSphereVertices(vertexData, kSubdivision, 0.5f);
    //  ビューポート
    D3D12_VIEWPORT viewport{};
    // クライアント領域のサイズと一緒にして画面全体に表示/
    viewport.Width = kClientWidth;
    viewport.Height = kClientHeight;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    //まずResourceを作成する。 １つあたりのIndexのサイズは32bitとするので、必要なサイズは32bit*6
    ID3D12Resource* indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);

    D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
    //リソースの先頭のアドレスから使う
    indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
    //使用するリソースのサイズはインデックス6つ分のサイズ
    indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
    //インデックスはuint32_tとする
    indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;
    //インデックス


    // シザー矩形
    D3D12_RECT scissorRect{};
    // 基本的にビューポートと同じ矩形が構成されるようにする
    scissorRect.left = 0;
    scissorRect.right = kClientWidth;
    scissorRect.top = 0;
    scissorRect.bottom = kClientHeight;
    // マテリアル用のリソースを作る今回はcolor一つ分のサイズを用意する
    ID3D12Resource* materialResource =
        CreateBufferResource(device, sizeof(Material));
    // マテリアルにデータを書き込む
    Material* materialData = nullptr;
    // 書き込むためのアドレスを取得
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    // 今回は赤を書き込んでみる
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->enableLighting = true;

    // WVPリソースを作る02_02
    ID3D12Resource* wvpResource =
        CreateBufferResource(device, sizeof(TransformationMatrix));
    // データを書き込む02_02
    TransformationMatrix* wvpData = nullptr;
    // 書き込むためのアドレスを取得02_02
    wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
    // 単位行列を書き込んでおく02_02
    wvpData->WVP = MakeIdentity4x4(); // WVPリソースを作る
    //wvpData->World = MakeAffineMatrix(transform.scale,transform.rotate,transform.translate); // WVPリソースを作る



    // sprite用のTransfomationMatrix用のリソースを作る。Matrix4x4
    // 1つ分のサイズを用意する04_00
    ID3D12Resource* transformationMatrixResourceSprite =
        CreateBufferResource(device, sizeof(TransformationMatrix));
    // sprite用のデータを書き込む04_00
    TransformationMatrix* transformationMatrixDataSprite = nullptr;
    // sprite用の書き込むためのアドレスを取得04_00
    transformationMatrixResourceSprite->Map(
        0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
    // 単位行列を書き込んでおく04_00
    transformationMatrixDataSprite->WVP = MakeIdentity4x4();
    transformationMatrixDataSprite->World = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);



    // ImGuiの初期化。詳細はさして重要ではないので解説は省略する。02_03
    // こういうもんである02_03
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsClassic();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(device, swapChainDesc.BufferCount, rtvDesc.Format,
        srvDescriptorHeap,
        srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());



    bool useMonstarBall = true;



    ID3D12Resource* directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
    DirectionalLight* directionalLightData = nullptr;
    directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
    directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData->intensity = 1.0f;
    directionalLightData->direction = Normalize(directionalLightData->direction);



    MSG msg{};

    // ウィンドウの×ボタンが押されるまでループ
    while (msg.message != WM_QUIT) {

        // Windowにメッセージが来てたら最優先で処理させる
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {


            transform.rotate.y += 0.03f; // X軸回転
            // ここがframeの先頭02_03
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            // 開発用UIの処理。実際に開発用のUIを出す場合はここをげ０無固有の処理を置き換える02_03
            ImGui::
                ShowDemoWindow(); // ImGuiの始まりの場所-----------------------------

            ImGui::Begin("Materialcolor");
            ImGui::SliderFloat3("Scale", &transform.scale.x, 0.1f, 5.0f);
            ImGui::SliderAngle("RotateX", &transform.rotate.x, -180.0f, 180.0f);
            ImGui::SliderAngle("RotateY", &transform.rotate.y, -180.0f, 180.0f);
            ImGui::SliderAngle("RotateZ", &transform.rotate.z, -180.0f, 180.0f);
            ImGui::SliderFloat3("Translate", &transform.translate.x, -5.0f, 5.0f);
            ImGui::ColorEdit4("Color", &(materialData->color.x));
            ImGui::SliderFloat3("TranslateSprite", &transformSprite.translate.x, -50.0f, 500.0f);
            ImGui::Checkbox("useMonstarBall", &useMonstarBall);
            ImGui::Text("useMonstarBall: %s", useMonstarBall ? "true" : "false");
            ImGui::DragFloat3("LightDirection", &directionalLightData->direction.x, 0.01f);
            ImGui::End();

            // ImGuiの内部コマンドを生成する02_03
            ImGui::
                Render(); // ImGui終わりの場所。描画の前02_03--------------------------

            // 描画用のDescrriptorHeapの設定02_03
            ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
            commandList->SetDescriptorHeaps(1, descriptorHeaps);

            // メイクアフィンマトリックス02_02
            Matrix4x4 worldMatrix = MakeAffineMatrix(
                transform.scale, transform.rotate, transform.translate);
            // カメラのメイクアフィンマトリックス02_02
            Matrix4x4 cameraMatrix =
                MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate,
                    cameraTransform.translate);
            // 逆行列カメラ02_02
            Matrix4x4 viewMatrix = Inverse(cameraMatrix);
            // 透視投影行列02_02
            Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
                0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);
            // ワールドビュープロジェクション行列02_02
            Matrix4x4 worldViewProjectionMatrix =
                Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
            // CBVのバッファに書き込む02_02
            wvpData->WVP = worldViewProjectionMatrix;
            wvpData->World = worldMatrix;

            // Sprite用のworldviewProjectionMatrixを作る04_00
            Matrix4x4 worldMatrixSprite =
                MakeAffineMatrix(transformSprite.scale, transformSprite.rotate,
                    transformSprite.translate);
            Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
            Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(
                0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
            Matrix4x4 woroldViewProjectionMatrixSprite =
                Multiply(worldMatrixSprite,
                    Multiply(viewMatrixSprite, projectionMatrixSprite));
            transformationMatrixDataSprite->WVP = woroldViewProjectionMatrixSprite;





        #pragma region 描画処理_クリアから描画まで
            // 画面のクリア処理//
            //   これから書き込むバックバッファのインデックスを取得
            UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

            // TransitionBarrieの設定(Present->RenderTarget)01_02
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = swapChainResources[backBufferIndex];
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            commandList->ResourceBarrier(1, &barrier);

            ///  描画先を設定（色と深度の両方）03_01
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
                dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false,
                &dsvHandle);
            // 指定した色で画面全体をクリアする
            float clearColor[] = {
                0.1f, 0.25f, 0.5f,
                1.0f }; /// 青っぽい色RGBAの順
            /// //これ最初の文字1.0fにするとピンク画面になる
            commandList->ClearRenderTargetView(rtvHandles[backBufferIndex],
                clearColor, 0, nullptr);
            commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH,
                1.0f, 0, 0, nullptr);

            // 描画設定
            commandList->RSSetViewports(1, &viewport);       // viewportを設定
            commandList->RSSetScissorRects(1, &scissorRect); // Scirssorを設定
            commandList->SetGraphicsRootSignature(rootSignature);
            commandList->SetPipelineState(graphicsPinelineState);     // PS0を設定
            commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // VBVを設定
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // マテリアルCbufferの場所を設定
            commandList->SetGraphicsRootConstantBufferView(
                0, materialResource->GetGPUVirtualAddress());
            // wvp用のCBufferの場所を設定02_02
            commandList->SetGraphicsRootConstantBufferView(
                1, wvpResource->GetGPUVirtualAddress());



            // シェーダー用リソース設定
            // SRV設定（ここで描画前にテクスチャ指定）
            if (useMonstarBall) {
                commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU2);
            } else {
                commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
            }


            commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

            commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());

            commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());



            // 球の描画（SRVが monsterBall または uvChecker）
            commandList->SetGraphicsRootDescriptorTable(
                2, useMonstarBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
            commandList->DrawInstanced(kNumVertices, 1, 0, 0);

            // ↓ Spriteの描画前に SRV を明示的に設定し直す（←これが大事）
            commandList->SetGraphicsRootDescriptorTable(
                2, textureSrvHandleGPU); // ← uvChecker 用

            // Sprite用VBV
            commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
            // CBuffer設定
            commandList->SetGraphicsRootConstantBufferView(
                0, materialResourceSprite->GetGPUVirtualAddress());
            commandList->SetGraphicsRootConstantBufferView(
                1, transformationMatrixResourceSprite->GetGPUVirtualAddress());


            // Spriteの描画（DrawInstanced）
            commandList->DrawInstanced(6, 1, 0, 0);
            // ここまで描画↑↑↑

            //   実際のcommandListのImGuiの描画コマンドを積む
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

            //  画面に描く処理は全て終わり,画面に映すので、状態を遷移01_02
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            // TransitionBarrierを張る
            commandList->ResourceBarrier(1, &barrier);
            // コマンドリストの内容を確定させる。すべ手のコマンドを積んでからCloseすること
            hr = commandList->Close();
            assert(SUCCEEDED(hr));

            // GPUにコマンドリストの実行を行わせる;
            ID3D12CommandList* commandLists[] = { commandList };
            commandQueue->ExecuteCommandLists(1, commandLists);
            // GPUとosに画面の交換を行うよう通知する
            swapChain->Present(1, 0);
            // Fenceの値を更新01_02
            fenceValue++;
            // GPUがじじなでたどり着いたときに,Fenceの値を指定した値に代入する01_02
            commandQueue->Signal(fence, fenceValue);
            // Fenceの値が指定したSignal値にたどりついているか確認する01_02
            // GetCompleteValueの初期値はFence作成時に渡した初期値01_02
            if (fence->GetCompletedValue() < fenceValue) {

                // 指定したSignalにたどり着いていないので,たどり着くまで待つようにイベントを設定する01_02
                fence->SetEventOnCompletion(fenceValue, fenceEvent);
                // イベント待つ01_02
                WaitForSingleObject(fenceEvent, INFINITE);
            }
            // 次のｆｒａｍｅ用のコマンドりイストを準備
            hr = commandAllocator->Reset();
            assert(SUCCEEDED(hr));
            hr = commandList->Reset(commandAllocator, nullptr);
            assert(SUCCEEDED(hr));
        }
    }
#pragma endregion

    // ImGuiの終了処理。詳細はさして重要ではないので解説は省略する。
    // こういうもんである。初期化と逆順に行う
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // 解放処理CG2_01_03
    CloseHandle(fenceEvent);
    fence->Release();
    rtvDescriptorHeap->Release();
    swapChainResources[0]->Release();
    swapChainResources[1]->Release();
    swapChain->Release();
    commandList->Release();
    commandAllocator->Release();
    commandQueue->Release();
    device->Release();
    useAdapter->Release();
    dxgiFactory->Release();
    vertexResource->Release();
    graphicsPinelineState->Release();
    signatureBlob->Release();
    if (errorBlob) {
        errorBlob->Release();
    }
    rootSignature->Release();
    pixelShaderBlob->Release();
    vertexShaderBlob->Release();
#ifdef _DEBUG
    debugController->Release();
    materialResource->Release();
    wvpResource->Release();
    srvDescriptorHeap->Release();
    textureResource->Release();      // 03_00
    mipImages.Release();             // 03_00
    dsvDescriptorHeap->Release();    // 03_01
    depthStencilResource->Release(); // 03_01
    textureResource2->Release();
    indexResourceSprite->Release();

    vertexResourceSprite->Release();
    transformationMatrixResourceSprite->Release();
    materialResourceSprite->Release();
    directionalLightResource->Release();


    CoInitialize(nullptr);
#endif
    CloseWindow(hwnd);

    // リソースチェックCG2_01_03
    IDXGIDebug1* debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
        debug->Release();
    }

    return 0;

}
