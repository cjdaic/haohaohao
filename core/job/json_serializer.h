#pragma once

#include "serializer.h"
#include <nlohmann/json.hpp>

namespace nbcam {

// JSON序列化器实现
class JsonSerializer : public ISerializer {
public:
    JsonSerializer() = default;
    ~JsonSerializer() override = default;
    
    std::string serialize(const LaserJob& job) override;
    std::unique_ptr<LaserJob> deserialize(const std::string& data) override;
    bool saveToFile(const LaserJob& job, const std::string& filepath) override;
    std::unique_ptr<LaserJob> loadFromFile(const std::string& filepath) override;

private:
    nlohmann::json jobToJson(const LaserJob& job) const;
    std::unique_ptr<LaserJob> jsonToJob(const nlohmann::json& json) const;
    nlohmann::json paramsToJson(const ProcessParams& params) const;
    ProcessParams jsonToParams(const nlohmann::json& json) const;
    nlohmann::json pointToJson(const PathPoint& point) const;
    PathPoint jsonToPoint(const nlohmann::json& json) const;
    nlohmann::json segmentToJson(const PathSegment& segment) const;
    PathSegment jsonToSegment(const nlohmann::json& json) const;
};

} // namespace nbcam
