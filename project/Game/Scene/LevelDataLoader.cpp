#include "LevelDataLoader.h"

#include "json.hpp"

#include <cassert>
#include <exception>
#include <fstream>

namespace {
using json = nlohmann::json;

Vector3 ReadVector3Array(const json& value, const Vector3& fallback) {
    if (!value.is_array() || value.size() < 3) {
        return fallback;
    }

    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>()
    };
}

Transform ReadBlenderTransform(const json& object) {
    Transform transform = {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };

    if (!object.contains("transform") || !object.at("transform").is_object()) {
        return transform;
    }

    const json& source = object.at("transform");
    const Vector3 translation = ReadVector3Array(source.value("translation", json::array()), transform.translate);
    const Vector3 rotation = ReadVector3Array(source.value("rotation", json::array()), transform.rotate);
    const Vector3 scaling = ReadVector3Array(source.value("scaling", json::array()), transform.scale);

    // Blender is Z-up while this game uses Y-up.
    // Mapping follows the assignment slides:
    //   game X <- Blender X
    //   game Y <- Blender Z
    //   game Z <- Blender Y
    transform.translate = { translation.x, translation.z, translation.y };
    transform.rotate = { -rotation.x, -rotation.z, rotation.y };
    transform.scale = { scaling.x, scaling.z, scaling.y };
    return transform;
}

Vector3 TransformPoint(const Vector3& point, const Transform& transform) {
    const Matrix4x4 matrix = Math::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
    return {
        point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0],
        point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1],
        point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2]
    };
}

Transform ComposeTransform(const Transform& parent, const Transform& local) {
    Transform world = {};

    // This engine stores Object3d transforms as separate scale / Euler rotation / translation values.
    // For level placement we keep that format and bake parent influence into the child at load time.
    world.scale = {
        parent.scale.x * local.scale.x,
        parent.scale.y * local.scale.y,
        parent.scale.z * local.scale.z
    };
    world.rotate = {
        parent.rotate.x + local.rotate.x,
        parent.rotate.y + local.rotate.y,
        parent.rotate.z + local.rotate.z
    };
    world.translate = TransformPoint(local.translate, parent);
    return world;
}

LevelObjectData ReadObjectRecursive(const json& object, const Transform& parentTransform) {
    assert(object.is_object());
    assert(object.contains("type"));

    LevelObjectData result;
    result.type = object.at("type").get<std::string>();
    result.name = object.value("name", result.type);
    result.fileName = object.value("file_name", "");
    result.transform = ComposeTransform(parentTransform, ReadBlenderTransform(object));

    if (object.contains("children") && object.at("children").is_array()) {
        const json& children = object.at("children");
        result.children.reserve(children.size());
        for (const json& child : children) {
            if (!child.is_object() || !child.contains("type")) {
                continue;
            }
            result.children.push_back(ReadObjectRecursive(child, result.transform));
        }
    }

    return result;
}
} // namespace

bool LevelDataLoader::Load(const std::string& filePath, LevelData& outLevelData, std::string* outStatus) {
    outLevelData = {};

    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            if (outStatus) {
                *outStatus = "Level load failed: cannot open file.";
            }
            return false;
        }

        json deserialized;
        file >> deserialized;

        if (!deserialized.is_object() ||
            !deserialized.contains("name") ||
            !deserialized.contains("objects") ||
            !deserialized.at("objects").is_array()) {
            if (outStatus) {
                *outStatus = "Level load failed: invalid level json format.";
            }
            return false;
        }

        outLevelData.name = deserialized.at("name").get<std::string>();
        const json& objects = deserialized.at("objects");
        outLevelData.objects.reserve(objects.size());
        const Transform rootTransform = {
            { 1.0f, 1.0f, 1.0f },
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f }
        };

        for (const json& object : objects) {
            if (!object.is_object() || !object.contains("type")) {
                if (outStatus) {
                    *outStatus = "Level load failed: object without type.";
                }
                return false;
            }
            outLevelData.objects.push_back(ReadObjectRecursive(object, rootTransform));
        }

        if (outStatus) {
            *outStatus = "Level loaded: " + filePath;
        }
        return true;
    } catch (const std::exception& e) {
        if (outStatus) {
            *outStatus = std::string("Level load failed: ") + e.what();
        }
        return false;
    }
}
