#include "D3DResourceLeakChecker.h"
#include <dxgidebug.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

// 終了時のDXGIデバッグ情報から、解放されていないGPU資源を報告する。
D3DResourceLeakChecker::~D3DResourceLeakChecker() {

    // 配布版では開発者向けの全資源列挙を省き、明示した計測時だけ有効にする。
#ifdef NDEBUG
    char requested[2]{};
    if (GetEnvironmentVariableA("CG2_REPORT_GPU_LEAKS", requested, 2) != 1 || requested[0] != '1') {
        return;
    }
#endif
    //リソースリークチェック
    Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
    // DXGIのデバッグインターフェースを取得
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
        // DXGI全体のリソースチェック（アプリが作ったリソースがまだ残ってるか確認）
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        // ALLに含まれるAPPとD3D12を重複して走査しない。
    }
}
