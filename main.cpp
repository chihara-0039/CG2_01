#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <format>
#include <string>
#include <dbghelp.h>
#include <strsafe.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <DirectXMath.h> 
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
using Vector4 = DirectX::XMFLOAT4; // Vector4の有効化
using Vector3 = DirectX::XMFLOAT3;
using Vector2 = DirectX::XMFLOAT2;

// ライブラリリンク
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")


struct Matrix4x4 {
    float m[4][4];
};

struct VertexData {
    Vector4 position;
    Vector2 texcoord;
};

Matrix4x4 MakeIdentity4x4() {
    Matrix4x4 mat = {};
    for (int i = 0; i < 4; i++) {
        mat.m[i][i] = 1.0f;
    }
    return mat;
}


struct Transform {
    Vector4 scale;
    Vector4 rotate;
    Vector4 translate;
};



Matrix4x4 Multiply(const Matrix4x4& mat1, const Matrix4x4& mat2) {
    Matrix4x4 result = {};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 4; ++k) {
                result.m[i][j] += mat1.m[i][k] * mat2.m[k][j];
            }
        }
    }
    return result;
}
Matrix4x4 worldMatrix = MakeIdentity4x4();

std::wstring ConvertString(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }

    auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
    if (sizeNeeded == 0) {
        return std::wstring();
    }
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
    return result;
}

std::string ConvertString(const std::wstring& str) {
    if (str.empty()) {
        return std::string();
    }

    auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
    if (sizeNeeded == 0) {
        return std::string();
    }
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
    return result;
}



// ログ関数（標準出力 + デバッグ出力）
void Log(std::ostream& os, const std::string& message) {
    os << message << std::endl;
    OutputDebugStringA(message.c_str());
}



IDxcBlob* CompileShader(

    const std::wstring& filePath,

    const wchar_t* profile,

    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler,
    std::ostream* logStream
) {
    Log(*logStream, ConvertString(std::format(L"Begin CompileShader, path: {}, profile:{}\n", filePath, profile)));
    IDxcBlobEncoding* shaderSource = nullptr;
    HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    assert(SUCCEEDED(hr));

    DxcBuffer shaderSourceBuffer{};
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;


    LPCWSTR arguments[] = {
        filePath.c_str(),
        L"-E", L"main",
        L"-T", profile,
        L"-Zi", L"-Qembed_debug",
        L"-Od",
        L"-Zpr"
    };

    IDxcResult* shaderResult = nullptr;

    hr = dxcCompiler->Compile(
        &shaderSourceBuffer,
        arguments,
        _countof(arguments),
        includeHandler,
        IID_PPV_ARGS(&shaderResult)
    );
    assert(SUCCEEDED(hr));

    IDxcBlobUtf8* shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);

    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        Log(*logStream, shaderError->GetStringPointer());
        assert(false);
    }

    IDxcBlob* shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    Log(*logStream, ConvertString(std::format(L" Compile Succeeded, path: {}, profile:{}\n", filePath, profile)));

    shaderSource->Release();
    shaderResult->Release();

    return shaderBlob;
}


// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }


    switch (msg) {
    case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {

    //時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力
    SYSTEMTIME time;
    GetLocalTime(&time);
    wchar_t filePath[MAX_PATH] = { 0 };
    CreateDirectory(L"./Dumps", nullptr);
    StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d_%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
    HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    //processId(このexenoId)とクラッシュ（例外）の発生したthreadIdを取得
    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();

    //設定情報を取得
    MINIDUMP_EXCEPTION_INFORMATION  minidumpInformation{ 0 };
    minidumpInformation.ThreadId = threadId;
    minidumpInformation.ExceptionPointers = exception;
    minidumpInformation.ClientPointers = TRUE;

    //Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
    MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

    //ほかに関連づけられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する
    return EXCEPTION_EXECUTE_HANDLER;

}

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {

    //頂点リソース用のヒープの設定
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    //頂点リソースの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource* resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource)
    );
    assert(SUCCEEDED(hr));
    return resource;
}

Matrix4x4 MakeAffineMatrix(const Vector4& scale, const Vector4& rotate, const Vector4& translate) {
    Matrix4x4 result = MakeIdentity4x4();

    // Scale  
    result.m[0][0] = scale.x;
    result.m[1][1] = scale.y;
    result.m[2][2] = scale.z;

    // Rotation (simplified, assuming rotation only around Y-axis for demonstration)  
    float cosY = cosf(rotate.y);
    float sinY = sinf(rotate.y);
    result.m[0][0] *= cosY;
    result.m[0][2] = sinY;
    result.m[2][0] = -sinY;
    result.m[2][2] *= cosY;

    // Translation  
    result.m[3][0] = translate.x;
    result.m[3][1] = translate.y;
    result.m[3][2] = translate.z;

    return result;
}

Matrix4x4 Inverse(const Matrix4x4& mat) {
    // Assuming the matrix is invertible and is a 4x4 matrix  
    Matrix4x4 result = {};
    float det =
        mat.m[0][0] * (mat.m[1][1] * mat.m[2][2] * mat.m[3][3] + mat.m[1][2] * mat.m[2][3] * mat.m[3][1] + mat.m[1][3] * mat.m[2][1] * mat.m[3][2]
            - mat.m[1][3] * mat.m[2][2] * mat.m[3][1] - mat.m[1][1] * mat.m[2][3] * mat.m[3][2] - mat.m[1][2] * mat.m[2][1] * mat.m[3][3])
        - mat.m[0][1] * (mat.m[1][0] * mat.m[2][2] * mat.m[3][3] + mat.m[1][2] * mat.m[2][3] * mat.m[3][0] + mat.m[1][3] * mat.m[2][0] * mat.m[3][2]
            - mat.m[1][3] * mat.m[2][2] * mat.m[3][0] - mat.m[1][0] * mat.m[2][3] * mat.m[3][2] - mat.m[1][2] * mat.m[2][0] * mat.m[3][3])
        + mat.m[0][2] * (mat.m[1][0] * mat.m[2][1] * mat.m[3][3] + mat.m[1][1] * mat.m[2][3] * mat.m[3][0] + mat.m[1][3] * mat.m[2][0] * mat.m[3][1]
            - mat.m[1][3] * mat.m[2][1] * mat.m[3][0] - mat.m[1][0] * mat.m[2][3] * mat.m[3][1] - mat.m[1][1] * mat.m[2][0] * mat.m[3][3])
        - mat.m[0][3] * (mat.m[1][0] * mat.m[2][1] * mat.m[3][2] + mat.m[1][1] * mat.m[2][2] * mat.m[3][0] + mat.m[1][2] * mat.m[2][0] * mat.m[3][1]
            - mat.m[1][2] * mat.m[2][1] * mat.m[3][0] - mat.m[1][0] * mat.m[2][2] * mat.m[3][1] - mat.m[1][1] * mat.m[2][0] * mat.m[3][2]);

    assert(det != 0.0f && "Matrix is not invertible");

    float invDet = 1.0f / det;

    // Compute the inverse matrix (manually calculated for 4x4 matrix)  
    // Fill in the result matrix with the appropriate calculations  
    // This is a placeholder; actual implementation would compute all 16 elements  
    result.m[0][0] = invDet * (mat.m[1][1] * mat.m[2][2] * mat.m[3][3] - mat.m[1][1] * mat.m[2][3] * mat.m[3][2]);
    // ... (continue for all elements of the matrix)  

    return result;
}


Matrix4x4 MakePerspetiveFovMatrix(float nearZ, float farZ, float fovY, float aspectRatio) {
    Matrix4x4 mat = {};
    float f = 1.0f / tanf(fovY * 0.5f);
    mat.m[0][0] = f / aspectRatio;
    mat.m[1][1] = f;
    mat.m[2][2] = farZ / (farZ - nearZ);
    mat.m[2][3] = 1.0f;
    mat.m[3][2] = -(farZ * nearZ) / (farZ - nearZ);
    return mat;
}

ID3D12DescriptorHeap* CreateDescriptorHeap(
    ID3D12Device* device,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    UINT numDescriptors,
    bool shaderVisible
) {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = type;
    heapDesc.NumDescriptors = numDescriptors;
    heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask = 0;

    ID3D12DescriptorHeap* descriptorHeap = nullptr;
    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));
    assert(SUCCEEDED(hr) && "Failed to create descriptor heap");

    return descriptorHeap;
}


// メイン関数
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    SetUnhandledExceptionFilter(ExportDump);

    // ログフォルダの作成とログファイル準備
    std::filesystem::create_directory("logs");

    //ここからファイルを作成しofstream
    auto now = std::chrono::system_clock::now();

    //ログファイルの名前にコンマ何秒はいらないので、削って秒にする
    auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);

    //日本時間(PCの設定時間)に変更
    std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };

    //formatを使って年月日_時分秒の文字列に変換
    std::string dateString = std::format("{:%Y-%m-%d %H-%M-%S}", localTime);

    //時刻を使ってファイル名を決定
    std::string logFilePath = std::string("logs/") + dateString + ".log";

    //ファイルを作って書き込み準備
    std::ofstream logStream(logFilePath);

    WNDCLASS wc{};
    //ウィンドウブロシージャ
    wc.lpfnWndProc = WindowProc;
    //ウィンドウクラス名(なんでもよい）
    wc.lpszClassName = L"CG2WindowClass";
    //インスタンスハンドル
    wc.hInstance = GetModuleHandle(nullptr);
    //カーソル
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    //ウィンドウクラスを登録する
    RegisterClass(&wc);

    //クライアント領域のサイズ
    const int32_t kClientWidth = 1280;
    const int32_t kClientHeight = 720;

    //ウィンドウサイズを表す構造体にクライアント領域を入れる
    RECT wrc = { 0, 0, kClientWidth, kClientHeight };

    //クライアント領域を元に実際のサイズにwrcを変更してもらう
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"CG2",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wrc.right - wrc.left,
        wrc.bottom - wrc.top,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr
    );

    ShowWindow(hwnd, SW_SHOW);


    //InputLayout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    //BlendStateの設定
    inputLayoutDesc.NumElements = _countof(inputElementDescs);


#ifdef _DEBUG
    ID3D12Debug1* debugController = nullptr;

    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {

        debugController->EnableDebugLayer();


        debugController->SetEnableGPUBasedValidation(TRUE);


    }


#endif



    // DXGIファクトリの作成
    IDXGIFactory7* dxgiFactory = nullptr;
    //HRESULTは	Windows系のエラーコードであり、
    //関数が成功したかどうかをSUCCEEDEDマクロで判定できる
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    //初期化の根本的な部分でエラーが出た場合はプログラムが間違ってるか、どうにもできない場合が多いのでassertにしておく
    assert(SUCCEEDED(hr));

    //使用することアダプタ用の変数。最初にnullptrを入れておく
    IDXGIAdapter4* useAdapter = nullptr;

    //いい順にアダプタを頼む
    for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
        i,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
        //アダプターの情報を取得
        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr));  //取得できないのは一大事

        //ソフトウェアアダプタでなければ採用
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            // Wide文字列をマルチバイトに変換
            char description[128];
            size_t convertedChars = 0;
            wcstombs_s(&convertedChars, description, adapterDesc.Description, _TRUNCATE);

            //採用したアダプタの情報をログに出力。wstringのほうなので注意
            Log(logStream, std::format("Use Adapter: {}\n", description));
            break;
        }
        useAdapter = nullptr;   //ソフトウェアアダプタの場合はみなかったことにする
    }

    //てきせつアダプタが見つからなかったので起動できない
    assert(useAdapter != nullptr);

    ID3D12Device* device = nullptr;


    //機能レベルとログ出力用の文字列
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
    };

    const char* featureLevelStrings[] = {
        "12.2",
        "12.1",
        "12.0"
    };

    //高い順にせいせいできるかためしていく
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        //採用したアダプターでデバイスを生成
        hr = D3D12CreateDevice(
            useAdapter,
            featureLevels[i],
            IID_PPV_ARGS(&device)
        );
        //指定した機能レベルでデバイスが生成できたか確認
        if (SUCCEEDED(hr)) {
            //生成できたのでログ出力を行ってループをぬける
            Log(logStream, std::format("FeatureLevel: {}\n", featureLevelStrings[i]));
            break;
        }
    }
    //デバイスの生成がうまくいかなかったのできどうできない
    assert(device != nullptr);
    //初期化完了のログを出す
    Log(logStream, "Complete create D3D12Device!!!\n");




#ifdef _DEBUG
    ID3D12InfoQueue* infoQueue = nullptr;

    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        // ヤバイエラー時に止まる  
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        // エラー時に止まる  
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        // 警告時に止まる  
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
        // 解放  
        infoQueue->Release();

        //抑制するメッセージのID
        D3D12_MESSAGE_ID denyIds[] = {

            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,

        };

        //抑制するレベル
        D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        //指定したメッセージの表示を抑制する
        infoQueue->PushStorageFilter(&filter);
    }

#endif  // DEBUG

    // コマンドキューの作成
    ID3D12CommandQueue* commandQueue = nullptr;
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    hr = device->CreateCommandQueue(
        &commandQueueDesc,
        IID_PPV_ARGS(&commandQueue)
    );
    assert(SUCCEEDED(hr));

    // コマンドアロケータの作成
    ID3D12CommandAllocator* commandAllocator = nullptr;
    hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator)
    );
    assert(SUCCEEDED(hr));

    // コマンドリストの作成
    ID3D12GraphicsCommandList* commandList = nullptr;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr,
    IID_PPV_ARGS(&commandList)
    );
    //コマンドキューの生成がうまくいかなかったのできどうできない
    assert(SUCCEEDED(hr));

    // スワップチェインの作成
    IDXGISwapChain4* swapChain = nullptr;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = kClientWidth; //画面の幅。ウィンドウのクライアント領域を同じものにしておく
    swapChainDesc.Height = kClientHeight;   //画面の高さ。ウィンドウのクライアント領域をおなじものにしておく
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1; //マルチサンプル
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;    //描画のターゲットとして利用する
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;   //モニターに移したら、中身を破壊

    //コマンドキュー、ウィンドウハンドル、設定を渡して生成する
    hr = dxgiFactory->CreateSwapChainForHwnd(
        commandQueue,
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1**>(&swapChain)
    );
    assert(SUCCEEDED(hr));

    //ディスクリプタヒープの生成
    //RTV用のヒープでディスクリプタの数は２。 RTVはShader内で触る物ではないので、ShaderVisibleはfalse
    ID3D12DescriptorHeap* rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
    
    D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
    rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDescriptorHeapDesc.NumDescriptors = 2;

    //hr = device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
    //assert(SUCCEEDED(hr));

    //SRV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなので、ShaderVisibleはture
    ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

    ID3D12Resource* swapChainResources[2] = { nullptr };
    hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));

    assert(SUCCEEDED(hr));
    hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
    assert(SUCCEEDED(hr));

    //RTVの設定
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

    rtvHandles[0] = rtvStartHandle;
    device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);

    rtvHandles[1].ptr = rtvStartHandle.ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);

    typedef struct D3D12_CPU_DESCRIPTOR_HANDLE {
        SIZE_T ptr;
    } D3D12_CPU_DESCRIPTOR_HANDLE;


    OutputDebugStringA("Hello, DirectX!\n");

    //初期化0でFence
    ID3D12Fence* fence = nullptr;
    uint64_t fenceValue = 0;
    hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    assert(SUCCEEDED(hr));

    //FenceのSignalを持つためのイベントを作成する
    HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, nullptr);
    assert(fenceEvent != nullptr);

    //dxcCompilerを初期化
    IDxcUtils* dxcUtils = nullptr;
    IDxcCompiler3* dxcCompiler = nullptr;
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
    assert(SUCCEEDED(hr));

    //現時点でincludeHandlerはしないが、includeに対応するための設定を行っておく
    IDxcIncludeHandler* includeHandler = nullptr;
    hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
    assert(SUCCEEDED(hr));

    //RootSignatureの作成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    //////////////////////////////////////////////////////////////////////////////////////////
   //RootParameter作成。複数設定できるので配列。今回は結果一つだけなので長さ１の配列
    D3D12_ROOT_PARAMETER rootParameters[2] = {};                            
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;       //CBVを使う
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;      //PixelShaderで使う
    rootParameters[0].Descriptor.ShaderRegister = 0;                        //レジスタ番号0使う
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;        //CBVを使う
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;    //VertexShaderで使う
    rootParameters[1].Descriptor.ShaderRegister = 0;                        //レジスタ番号0使う
    descriptionRootSignature.pParameters = rootParameters;                  //ルートパラメータ配列へのポインタ
    descriptionRootSignature.NumParameters = _countof(rootParameters);      //配列の長さ


    //シリアライズしてバイナリにする
    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);

    }

    //バイナリを元に生成
    ID3D12RootSignature* rootSignature = nullptr;
    hr = device->CreateRootSignature(0,
        signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature));

    assert(SUCCEEDED(hr));


    //BlendStateの設定
    D3D12_BLEND_DESC blendDesc{};

    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    //ResiterzerStartの設定
    D3D12_RASTERIZER_DESC rasterizerDesc{};

    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    //Shaderをコンパイルする
    IDxcBlob* vertexShaderBlob = CompileShader(L"Object3D.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler, &logStream);
    assert(vertexShaderBlob != nullptr);

    IDxcBlob* pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler, &logStream);
    assert(pixelShaderBlob != nullptr);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature;// RootSignature
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;// InputLayout
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
    vertexShaderBlob->GetBufferSize() };// VertexShader
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };// PixelShader 
    graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;// RasterizerState
    // 書き込むRTVの情報
    graphicsPipelineStateDesc.
        NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    // 利用するトポロジ (形状) のタイプ。 三角形 
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // どのように画面に色を打ち込むかの設定 (気にしなくて良い) 
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    // 実際に生成
    ID3D12PipelineState* graphicsPipelineState = nullptr;
    hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
        IID_PPV_ARGS(&graphicsPipelineState));
    assert(SUCCEEDED(hr));

    // 実際に頂点リソースを作る
    ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * 3);


    // 頂点バッファビューを作成する
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

    // リソースの先頭のアドレスから使う
    vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
    // 使用するリソースのサイズは頂点3つ分のサイズ
    vertexBufferView.SizeInBytes = sizeof(VertexData) * 3;
    // 1頂点あたりのサイズ
    vertexBufferView.StrideInBytes = sizeof(VertexData);



    // 頂点リソースにデータを書き込む 
    VertexData* vertexData = nullptr; 

    // 書き込むためのアドレスを取得
    vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    //左下
    vertexData[0].position = { -0.5f, -0.5f, 0.0f, 1.0f };
    vertexData[0].texcoord = { 0.0f, 1.0f };

    // 上
    vertexData[1].position = { 0.0f, 0.5f, 0.0f, 1.0f };
    vertexData[1].texcoord = { 0.5f, 0.0f };

    // 右下
    vertexData[2].position = { 0.5f, -0.5f, 0.0f, 1.0f };
    vertexData[2].texcoord = { 1.0f, 1.0f };

    //マテリアル用のリソースを作る。今回はcolor一つ分のサイズを用意する
    ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Vector4));

    //マテリアルにデータを書き込む
    Vector4* materialData = nullptr;

    //書き込むためのアドレスを取得
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

    //マテリアルにデータを書き込む
    *materialData = Vector4{ 1.0f, 0.0f, 0.0f, 1.0f };


    //WVP用のリソースを作る。Material4x4 1つ分のサイズを用意する
    ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(Matrix4x4));


    Transform transform{ {1.0f,1.0f,1.0f,0.0f}, {0.0f,0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f,0.0f} };
    // Transform cameratransform{ {1.0f,1.0f,1.0f,0.0f}, {0.0f,0.0f,0.0f,0.0f}, {0.0f,0.0f,-5.0f,0.0f} };
     // 修正: "cameraTransform" が未定義のため、適切な定義を追加します。
    Transform cameraTransform = { {1.0f, 1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -5.0f, 0.0f} };
   
    // データを書き込む
    Matrix4x4* wvpData = nullptr;

    //書き込む溜めのアドレスを取得
    wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));

    //単位行列を書き込んでおく
    *wvpData = MakeIdentity4x4();

    Matrix4x4* transformationMatrixData = nullptr;
    wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));



    // ビューポート
    D3D12_VIEWPORT viewport{};
    viewport.Width = kClientWidth;
    viewport.Height = kClientHeight;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // シザー矩形
    D3D12_RECT scissorRect{};
    // 基本的にビューポートと同じ矩形が構成されるようにする
    scissorRect.left = 0;
    scissorRect.right = kClientWidth;
    scissorRect.top = 0;
    scissorRect.bottom = kClientHeight;

    /////////////////////////////////

    // ImGuiの初期化。詳細はさして重要ではないので解説は省略する。
    //こういうもんである
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsClassic();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(device,
        swapChainDesc.BufferCount,
        rtvDesc.Format,
        srvDescriptorHeap,
        srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());



    // メッセージループ
    MSG msg{};
    //ウィンドウの×ボタンが押されるまでループ
    while (msg.message != WM_QUIT) {
        //Windowにメッセージが来てたら最優先で処理させる
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        //これから書き込むバックバッファののインデックスを取得
        UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

        //TransitionBarrierの設定
        D3D12_RESOURCE_BARRIER barrier{};

        //今回のバリアはTransition
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

        //Noneにしておく
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

        //バリアを張る対象のリソース。現在のバックバッファに対して行う
        barrier.Transition.pResource = swapChainResources[backBufferIndex];

        //遷移前（現在）のResourceStarte
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;

        //遷移後の	ResourceStarte
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        //TransitionBarrierを張る
        commandList->ResourceBarrier(1, &barrier);

        //描画先のRTVを設定する
        commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);

        //指定した色で画面全体をクリアする
        float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
        commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

        //描画用のDescriptorHeapの設定
        ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
        commandList->SetDescriptorHeaps(1, descriptorHeaps);

        commandList->RSSetViewports(1, &viewport); //Viewportを設定
        commandList->RSSetScissorRects(1, &scissorRect); //Scirssorを設定
        // RootSignatureを設定。PSOに設定しているけど別途設定が必要 
        commandList->SetGraphicsRootSignature(rootSignature);
        commandList->SetPipelineState(graphicsPipelineState);// PSOを設定
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);// VBVを設定
        // 形状を設定。 PSOに設定しているものとはまた別。 同じものを設定すると考えておけば良い 
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

        //wvp用のCBufferの場所を設定
        commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());


        transform.rotate.y += 0.03f;


        Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
        Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
        Matrix4x4 viewMatrix = Inverse(cameraMatrix);
        Matrix4x4 projectionMatrix = MakePerspetiveFovMatrix(0.45f, (float)kClientWidth / (float)kClientHeight, 0.1f, 100.0f);
        //WVPMatrixを作る
        Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

        // Matrix4x4* transformationMatrixData = nullptr;

        *transformationMatrixData = worldViewProjectionMatrix;


        *wvpData = worldMatrix;

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Triangle Color");
        ImGui::ColorEdit4("Color", reinterpret_cast<float*>(materialData));
        ImGui::End();

        //開発用UIの処理。 実際に開発用のUIを出す場合はここをゲーム固有のそりに置き換える
        ImGui::ShowDemoWindow();

        //ImGuiの内部コマンドを生成する
        ImGui::Render();



        // 描画! (DrawCall/ ドローコール)。 3頂点で1つのインスタンス。 インスタンスについては今後 
        commandList->DrawInstanced(3, 1, 0, 0);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

        commandList->ResourceBarrier(1, &barrier);

        hr = commandList->Close();
        assert(SUCCEEDED(hr));


        // コマンドリストを実行
        ID3D12CommandList* commandLists[] = { commandList };
        commandQueue->ExecuteCommandLists(1, commandLists);

        swapChain->Present(1, 0);

        fenceValue++;

        commandQueue->Signal(fence, fenceValue);



        //Fenceの値が指定したSignal値にたどり着いているか確認すること
            //GetCompletedValueの初期値はFence作成時に渡した初期値
        if (fence->GetCompletedValue() < fenceValue) {

            //指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
            fence->SetEventOnCompletion(fenceValue, fenceEvent);

            //イベント待つ
            WaitForSingleObject(fenceEvent, INFINITE);
        }

        //次のフレーム用のコマンドリストのを準備
        hr = commandAllocator->Reset();
        assert(SUCCEEDED(hr));
        hr = commandList->Reset(commandAllocator, nullptr);
        assert(SUCCEEDED(hr));


    }


    CloseHandle(fenceEvent);
    fence->Release();
    rtvDescriptorHeap->Release();
    srvDescriptorHeap->Release();
    swapChainResources[0]->Release();
    swapChainResources[1]->Release();
    swapChain->Release();
    commandList->Release();
    commandAllocator->Release();
    commandQueue->Release();
    device->Release();
    useAdapter->Release();
    dxgiFactory->Release();
#ifdef _DEBUG
    debugController->Release();
#endif
    CloseWindow(hwnd);

    vertexResource->Release();
    graphicsPipelineState->Release(); signatureBlob->Release();
    if (errorBlob) {
        errorBlob->Release();
    }
    rootSignature->Release();
    pixelShaderBlob->Release();
    vertexShaderBlob->Release();
    materialResource->Release();
    wvpResource->Release();
    includeHandler->Release();
    dxcUtils->Release();
    dxcCompiler->Release();


    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    //リソースリークチェック
    IDXGIDebug1* debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
        debug->Release();
    }



    return 0;
}