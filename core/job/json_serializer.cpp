#include "json_serializer.h"
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>

namespace nbcam {

std::string JsonSerializer::serialize(const LaserJob& job) {
    return jobToJson(job).dump(2);  // 缩进2个空格，便于阅读
}

std::unique_ptr<LaserJob> JsonSerializer::deserialize(const std::string& data) {
    try {
        nlohmann::json json = nlohmann::json::parse(data);
        if (json.is_object() && json.contains("laser_job") && json["laser_job"].is_object()) {
            return jsonToJob(json["laser_job"]);
        }
        return jsonToJob(json);
    } catch (const std::exception& e) {
        spdlog::error("JSON反序列化失败: {}", e.what());
        return nullptr;
    }
}

bool JsonSerializer::saveToFile(const LaserJob& job, const std::string& filepath) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("无法打开文件: {}", filepath);
            return false;
        }
        
        std::string json_str = serialize(job);
        file << json_str;
        file.close();
        return true;
    } catch (const std::exception& e) {
        spdlog::error("保存文件失败: {}", e.what());
        return false;
    }
}

std::unique_ptr<LaserJob> JsonSerializer::loadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            spdlog::error("无法打开文件: {}", filepath);
            return nullptr;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        return deserialize(buffer.str());
    } catch (const std::exception& e) {
        spdlog::error("加载文件失败: {}", e.what());
        return nullptr;
    }
}

nlohmann::json JsonSerializer::jobToJson(const LaserJob& job) const {
    nlohmann::json json;
    
    // meta
    json["meta"]["job_id"] = job.meta.job_id;
    json["meta"]["units"] = job.meta.units;
    json["meta"]["source_model"] = job.meta.source_model;
    json["meta"]["created_at"] = job.meta.created_at;
    
    // coordinate
    json["coordinate"]["workplane"] = job.coordinate.workplane;
    json["coordinate"]["machine_frame"] = job.coordinate.machine_frame;
    json["coordinate"]["transform_model_to_machine"] = job.coordinate.transform_model_to_machine;
    
    // parameterization
    json["parameterization"]["algorithm"] = job.parameterization.algorithm;
    json["parameterization"]["uv_island_count"] = job.parameterization.uv_island_count;
    json["parameterization"]["notes"] = job.parameterization.notes;
    
    // process_defaults
    json["process_defaults"] = paramsToJson(job.process_defaults);
    
    // segments
    json["segments"] = nlohmann::json::array();
    for (const auto& segment : job.segments) {
        json["segments"].push_back(segmentToJson(segment));
    }
    
    return json;
}

std::unique_ptr<LaserJob> JsonSerializer::jsonToJob(const nlohmann::json& json) const {
    auto job = std::make_unique<LaserJob>();
    
    // meta
    if (json.contains("meta")) {
        job->meta.job_id = json["meta"].value("job_id", "");
        job->meta.units = json["meta"].value("units", "mm");
        job->meta.source_model = json["meta"].value("source_model", "");
        job->meta.created_at = json["meta"].value("created_at", "");
    }
    
    // coordinate
    if (json.contains("coordinate")) {
        job->coordinate.workplane = json["coordinate"].value("workplane", "z=0");
        job->coordinate.machine_frame = json["coordinate"].value("machine_frame", "laser_head_top");
        if (json["coordinate"].contains("transform_model_to_machine")) {
            job->coordinate.transform_model_to_machine = json["coordinate"]["transform_model_to_machine"].get<std::vector<double>>();
        }
    }
    
    // parameterization
    if (json.contains("parameterization")) {
        job->parameterization.algorithm = json["parameterization"].value("algorithm", "ABF");
        job->parameterization.uv_island_count = json["parameterization"].value("uv_island_count", 1);
        job->parameterization.notes = json["parameterization"].value("notes", "");
    }
    
    // process_defaults
    if (json.contains("process_defaults")) {
        job->process_defaults = jsonToParams(json["process_defaults"]);
    }
    
    // segments
    if (json.contains("segments") && json["segments"].is_array()) {
        for (const auto& seg_json : json["segments"]) {
            job->segments.push_back(std::move(jsonToSegment(seg_json)));
        }
    }
    
    return job;
}

nlohmann::json JsonSerializer::paramsToJson(const ProcessParams& params) const {
    nlohmann::json json;
    json["power_w"] = params.power_w;
    json["freq_hz"] = params.freq_hz;
    json["speed_mm_s"] = params.speed_mm_s;
    json["laser_on_delay_us"] = params.laser_on_delay_us;
    json["laser_off_delay_us"] = params.laser_off_delay_us;
    return json;
}

ProcessParams JsonSerializer::jsonToParams(const nlohmann::json& json) const {
    ProcessParams params;
    params.power_w = json.value("power_w", 20.0);
    params.freq_hz = json.value("freq_hz", 20000.0);
    params.speed_mm_s = json.value("speed_mm_s", 300.0);
    params.laser_on_delay_us = json.value("laser_on_delay_us", 0);
    params.laser_off_delay_us = json.value("laser_off_delay_us", 0);
    return params;
}

nlohmann::json JsonSerializer::pointToJson(const PathPoint& point) const {
    nlohmann::json json;
    json["u"] = point.u;
    json["v"] = point.v;
    json["x"] = point.x;
    json["y"] = point.y;
    json["z"] = point.z;
    json["a"] = point.a;
    json["b"] = point.b;
    json["laser"] = point.laser;
    
    if (point.params_override) {
        json["power_w"] = point.params_override->power_w;
        json["freq_hz"] = point.params_override->freq_hz;
        json["speed_mm_s"] = point.params_override->speed_mm_s;
        json["laser_on_delay_us"] = point.params_override->laser_on_delay_us;
        json["laser_off_delay_us"] = point.params_override->laser_off_delay_us;
    }
    
    return json;
}

PathPoint JsonSerializer::jsonToPoint(const nlohmann::json& json) const {
    PathPoint point;
    point.u = json.value("u", 0.0);
    point.v = json.value("v", 0.0);
    point.x = json.value("x", 0.0);
    point.y = json.value("y", 0.0);
    point.z = json.value("z", 0.0);
    point.a = json.value("a", 0.0);
    point.b = json.value("b", 0.0);
    point.laser = json.value("laser", 0);
    
    // 检查是否有点级参数覆盖
    if (json.contains("power_w") || json.contains("freq_hz") || json.contains("speed_mm_s") ||
        json.contains("laser_on_delay_us") || json.contains("laser_off_delay_us")) {
        point.params_override = std::make_unique<ProcessParams>();
        point.params_override->power_w = json.value("power_w", 20.0);
        point.params_override->freq_hz = json.value("freq_hz", 20000.0);
        point.params_override->speed_mm_s = json.value("speed_mm_s", 300.0);
        point.params_override->laser_on_delay_us = json.value("laser_on_delay_us", 0);
        point.params_override->laser_off_delay_us = json.value("laser_off_delay_us", 0);
    }
    
    return point;
}

nlohmann::json JsonSerializer::segmentToJson(const PathSegment& segment) const {
    nlohmann::json json;
    json["id"] = segment.id;
    
    // type
    json["type"] = (segment.type == SegmentType::MARK) ? "mark" : "jump";
    
    // strategy
    std::string strategy_str;
    switch (segment.strategy) {
        case FillStrategy::CONTOUR: strategy_str = "contour"; break;
        case FillStrategy::HATCH: strategy_str = "hatch"; break;
        case FillStrategy::RING: strategy_str = "ring"; break;
        case FillStrategy::IMAGE_SAMPLE: strategy_str = "image_sample"; break;
        case FillStrategy::ARC_HATCH: strategy_str = "arc_hatch"; break;
    }
    json["strategy"] = strategy_str;
    
    // params_override
    if (segment.params_override) {
        json["params_override"] = paramsToJson(*segment.params_override);
    }
    
    // points
    json["points"] = nlohmann::json::array();
    for (const auto& point : segment.points) {
        json["points"].push_back(pointToJson(point));
    }
    
    return json;
}

PathSegment JsonSerializer::jsonToSegment(const nlohmann::json& json) const {
    PathSegment segment;
    segment.id = json.value("id", 0);
    
    // type
    std::string type_str = json.value("type", "mark");
    segment.type = (type_str == "mark") ? SegmentType::MARK : SegmentType::JUMP;
    
    // strategy
    std::string strategy_str = json.value("strategy", "contour");
    if (strategy_str == "hatch") {
        segment.strategy = FillStrategy::HATCH;
    } else if (strategy_str == "arc_hatch") {
        segment.strategy = FillStrategy::ARC_HATCH;
    } else if (strategy_str == "ring") {
        segment.strategy = FillStrategy::RING;
    } else if (strategy_str == "image_sample") {
        segment.strategy = FillStrategy::IMAGE_SAMPLE;
    } else {
        segment.strategy = FillStrategy::CONTOUR;
    }
    
    // params_override
    if (json.contains("params_override")) {
        segment.params_override = std::make_unique<ProcessParams>(jsonToParams(json["params_override"]));
    }
    
    // points
    if (json.contains("points") && json["points"].is_array()) {
        for (const auto& point_json : json["points"]) {
            segment.points.push_back(std::move(jsonToPoint(point_json)));
        }
    }
    
    return segment;
}

} // namespace nbcam
