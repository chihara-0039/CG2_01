#include "Model.h"
#include <fstream>
#include <sstream>
#include <cassert>

using namespace Microsoft::WRL;

Model* Model::CreateFromOBJ(DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename, TextureManager* textureManager) {
    Model* model = new Model();
    model->Initialize(dxCommon, directoryPath, filename, textureManager);
    return model;
}

void Model::Initialize(DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename, TextureManager* textureManager) {
    // 1. OBJ読み込み
    LoadObjFile(directoryPath, filename);

    // ★安全策：頂点が一つもない場合はバッファを作らない
    if (vertices_.empty()) {
        OutputDebugStringA("Error: Model vertices are empty!\n");
        return;
    }

    // 2. バッファ生成
    CreateBuffers(dxCommon);

    // 3. テクスチャ読み込み
    if (textureManager) {
        // パスが不安な場合は、ここでフルパスを組み立てるか確認ログを出す
        textureHandle_ = textureManager->LoadTexture("Resources/uvChecker.png");
    }
}

void Model::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    std::string fullPath = directoryPath + "/" + filename;
    std::ifstream file(fullPath);
    assert(file.is_open());

    // ★追加：どのパスのファイルを読んでいるか出力
    OutputDebugStringA(("---- Loading: " + fullPath + " ----\n").c_str());

    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::string line;
    int lineCount = 0; // ★追加：何行読めたかカウント

    while (std::getline(file, line)) {
        lineCount++; // 行数をカウント
        std::stringstream ss(line);
        std::string identifier;
        ss >> identifier;

        if (identifier == "v") {
            Vector4 position;
            ss >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            positions.push_back(position);
        } else if (identifier == "vt") {
            Vector2 texcoord;
            ss >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);
        } else if (identifier == "vn") {
            Vector3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } else if (identifier == "f") {
            for (int i = 0; i < 3; i++) {
                std::string s;
                ss >> s;
                std::stringstream ss2(s);
                std::string idx;
                int p = 0, t = 0, n = 0;

                std::getline(ss2, idx, '/'); if (!idx.empty()) p = std::stoi(idx) - 1;
                std::getline(ss2, idx, '/'); if (!idx.empty()) t = std::stoi(idx) - 1;
                std::getline(ss2, idx, '/'); if (!idx.empty()) n = std::stoi(idx) - 1;

                ModelVertexData v;
                v.position = positions[p];
                if (t >= 0 && t < texcoords.size()) v.texcoord = texcoords[t];
                if (n >= 0 && n < normals.size()) v.normal = normals[n];

                v.position.z *= -1.0f;
                v.normal.z *= -1.0f;

                // ★カリング対策：Z反転に伴い、面を裏返さないためにここで 0 -> 2 -> 1 の順に後でなるよう対策
                // とりあえず今回はそのまま入れてみる
                vertices_.push_back(v);
            }
        }
    }

    // ★追加：最終結果のレポートを出力
    std::string report = "Read Lines: " + std::to_string(lineCount) + ", Parsed Vertices: " + std::to_string(vertices_.size()) + "\n";
    OutputDebugStringA(report.c_str());

    if (vertices_.empty()) {
        OutputDebugStringA(("Error: Model vertices are empty! File: " + filename + "\n").c_str());
    }
}

void Model::CreateBuffers(DirectXCommon* dxCommon) {
    auto device = dxCommon->GetDevice();
    UINT sizeIB = static_cast<UINT>(sizeof(ModelVertexData) * vertices_.size());

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeIB; // ここが0だとCreateCommittedResourceは失敗する
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    // ★重要：HRESULTで成功を確認する
    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer_));

    if (FAILED(hr)) {
        assert(false && "Failed to create Vertex Buffer");
        return;
    }

    // データ転送
    ModelVertexData* vertMap = nullptr;
    hr = vertexBuffer_->Map(0, nullptr, (void**)&vertMap);
    if (SUCCEEDED(hr)) {
        std::copy(vertices_.begin(), vertices_.end(), vertMap);
        vertexBuffer_->Unmap(0, nullptr);
    }

    // View作成
    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeIB;
    vertexBufferView_.StrideInBytes = sizeof(ModelVertexData);
}
void Model::Draw(ID3D12GraphicsCommandList* commandList) {
    // ★安全策：バッファがない場合は描画しない
    if (!vertexBuffer_) {
        return;
    }

    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
}