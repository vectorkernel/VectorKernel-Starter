#pragma once
#include <vector>
#include <string>
#include <atomic>
#include <json/json.h>
#include <glm/glm.hpp>
#include "BoundingBox.h"
#include <algorithm>
#include <limits>

class DataEntity {
public:
    virtual float distanceTo(const glm::vec2& point) const = 0;
    inline DataEntity() : id_(generateId()), drawOrder_(0) {}
    inline virtual ~DataEntity() {}

    inline virtual std::string getType() const = 0;
    inline virtual Json::Value getJSON() const = 0;
    inline virtual const std::vector<glm::vec3>& getVertices() const = 0;
    inline virtual const glm::vec4& getColor() const {
        static glm::vec4 defaultColor(1.0f, 1.0f, 1.0f, 1.0f);
        return defaultColor;
    }
    inline virtual float getWidth() const { return 1.0f; }

    inline std::size_t getId() const { return id_; }
    inline int getDrawOrder() const { return drawOrder_; }
    inline void setDrawOrder(int order) { drawOrder_ = order; }

    virtual BoundingBox getBounds() const {
        BoundingBox bb;
        bb.minX = std::numeric_limits<float>::max();
        bb.minY = std::numeric_limits<float>::max();
        bb.minZ = std::numeric_limits<float>::max();
        bb.maxX = std::numeric_limits<float>::lowest();
        bb.maxY = std::numeric_limits<float>::lowest();
        bb.maxZ = std::numeric_limits<float>::lowest();
        for (const auto& v : getVertices()) {
            bb.minX = std::min(bb.minX, v.x);
            bb.minY = std::min(bb.minY, v.y);
            bb.minZ = std::min(bb.minZ, v.z);
            bb.maxX = std::max(bb.maxX, v.x);
            bb.maxY = std::max(bb.maxY, v.y);
            bb.maxZ = std::max(bb.maxZ, v.z);
        }
        return bb;
    }

private:
    std::size_t id_;
    int drawOrder_;
    inline static std::size_t generateId() {
        static std::atomic<std::size_t> counter{ 0 };
        return ++counter;
    }
};
