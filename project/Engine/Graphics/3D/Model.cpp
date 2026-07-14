#include "Model.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <unordered_map>
#include <stdexcept>

using namespace Microsoft::WRL;

std::unique_ptr<Model> Model::CreateFromOBJ(DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename, TextureManager* textureManager) {
    std::unique_ptr<Model> model = std::make_unique<Model>();
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
        textureHandle_ = textureManager->LoadTexture(textureFilePath_);
    }
}

void Model::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    std::string fullPath = directoryPath + "/" + filename;
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        const std::string message = "Failed to open OBJ file: " + fullPath;
        OutputDebugStringA((message + "\n").c_str());
        throw std::runtime_error(message);
    }

    // ★追加：どのパスのファイルを読んでいるか出力
    OutputDebugStringA(("---- Loading: " + fullPath + " ----\n").c_str());

    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::unordered_map<std::string, uint32_t> vertexIndexMap;
    std::string line;
    int lineCount = 0; // ★追加：何行読めたかカウント

    vertices_.clear();
    indices_.clear();

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

                auto existing = vertexIndexMap.find(s);
                if (existing != vertexIndexMap.end()) {
                    indices_.push_back(existing->second);
                    continue;
                }

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

                uint32_t newIndex = static_cast<uint32_t>(vertices_.size());
                vertexIndexMap.emplace(s, newIndex);
                vertices_.push_back(v);
                indices_.push_back(newIndex);
            }
        } else if (identifier == "mtllib") {
            std::string mtlFilename;
            ss >> mtlFilename;
            LoadMaterialFile(directoryPath, mtlFilename);
        }
    }

    // ★追加：最終結果のレポートを出力
    std::string report = "Read Lines: " + std::to_string(lineCount) + ", Parsed Vertices: " + std::to_string(vertices_.size()) + ", Parsed Indices: " + std::to_string(indices_.size()) + "\n";
    OutputDebugStringA(report.c_str());

    if (vertices_.empty()) {
        OutputDebugStringA(("Error: Model vertices are empty! File: " + filename + "\n").c_str());
    }
}

void Model::CreateBuffers(DirectXCommon* dxCommon) {
    auto device = dxCommon->GetDevice();
    UINT sizeVB = static_cast<UINT>(sizeof(ModelVertexData) * vertices_.size());

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeVB; // ここが0だとCreateCommittedResourceは失敗する
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
    vertexBufferView_.SizeInBytes = sizeVB;
    vertexBufferView_.StrideInBytes = sizeof(ModelVertexData);

    if (!indices_.empty()) {
        UINT sizeIB = static_cast<UINT>(sizeof(uint32_t) * indices_.size());
        resDesc.Width = sizeIB;

        hr = device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBuffer_));

        if (FAILED(hr)) {
            assert(false && "Failed to create Index Buffer");
            return;
        }

        uint32_t* indexMap = nullptr;
        hr = indexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&indexMap));
        if (SUCCEEDED(hr)) {
            std::copy(indices_.begin(), indices_.end(), indexMap);
            indexBuffer_->Unmap(0, nullptr);
        }

        indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes = sizeIB;
        indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    }
}

void Model::LoadMaterialFile(const std::string& directoryPath, const std::string& filename) {
    std::string fullPath = directoryPath + "/" + filename;
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        OutputDebugStringA(("Warning: Failed to open material file: " + fullPath + "\n").c_str());
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string identifier;
        ss >> identifier;

        if (identifier == "map_Kd") {
            std::string textureFilename;
            ss >> textureFilename;

            textureFilePath_ = directoryPath + "/" + textureFilename;
            OutputDebugStringA(("Texture from MTL: " + textureFilePath_ + "\n").c_str());
            return;
        }
    }
}

void Model::InitializeFromVertices(DirectXCommon* dxCommon, const std::vector<ModelVertexData>& vertices, uint32_t textureHandle) {
    vertices_ = vertices;
    indices_.clear();
    textureHandle_ = textureHandle;
    
    if (vertices_.empty()) {
        OutputDebugStringA("Error: Vertices empty in InitializeFromVertices!\n");
        return;
    }
    
    CreateBuffers(dxCommon);
}

void Model::UpdateVertexBuffer(const std::vector<ModelVertexData>& vertices) {
    // 頂点数が一致することを確認 (またはリサイズが必要だが、スキニングでは不変)
    if (vertices.size() != vertices_.size()) {
        OutputDebugStringA("Warning: Vertex count mismatch in UpdateVertexBuffer!\n");
        // 念のため更新後のサイズに合わせる
        vertices_ = vertices;
        return;
    }
    
    vertices_ = vertices;
    if (!vertexBuffer_) {
        return;
    }
    
    ModelVertexData* vertMap = nullptr;
    HRESULT hr = vertexBuffer_->Map(0, nullptr, (void**)&vertMap);
    if (SUCCEEDED(hr)) {
        std::copy(vertices_.begin(), vertices_.end(), vertMap);
        vertexBuffer_->Unmap(0, nullptr);
    }
}

void Model::Draw(ID3D12GraphicsCommandList* commandList) {
    // ★安全策：バッファがない場合は描画しない
    if (!vertexBuffer_) {
        return;
    }

    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    if (indexBuffer_ && !indices_.empty()) {
        commandList->IASetIndexBuffer(&indexBufferView_);
        commandList->DrawIndexedInstanced(UINT(indices_.size()), 1, 0, 0, 0);
    } else {
        commandList->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
    }
}

void Model::DrawInstanced(ID3D12GraphicsCommandList* commandList, UINT instanceCount) {
    if (!vertexBuffer_ || instanceCount == 0) {
        return;
    }

    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    if (indexBuffer_ && !indices_.empty()) {
        commandList->IASetIndexBuffer(&indexBufferView_);
        commandList->DrawIndexedInstanced(UINT(indices_.size()), instanceCount, 0, 0, 0);
    } else {
        commandList->DrawInstanced(UINT(vertices_.size()), instanceCount, 0, 0);
    }
}

