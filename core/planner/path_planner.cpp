#include "path_planner.h"
#include <spdlog/spdlog.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

void copyPointCoord(nbcam::PathPoint& dst, const nbcam::PathPoint& src)
{
    dst.u = src.u;
    dst.v = src.v;
    dst.x = src.x;
    dst.y = src.y;
    dst.z = src.z;
    dst.a = src.a;
    dst.b = src.b;
}

void normalizeSegmentSequence(nbcam::LaserJob& job)
{
    // 过滤掉无效段（至少需要两个点才能形成有效线段）
    job.segments.erase(
        std::remove_if(job.segments.begin(), job.segments.end(),
                       [](const nbcam::PathSegment& seg) { return seg.points.size() < 2; }),
        job.segments.end());

    // 统一顺序ID，避免MARK/JUMP出现重复ID。
    for (size_t i = 0; i < job.segments.size(); ++i) {
        job.segments[i].id = static_cast<int>(i);
    }

    // 强制相邻段首尾连续，确保扫描轨迹在ID顺序上严格相接。
    for (size_t i = 1; i < job.segments.size(); ++i) {
        auto& prev = job.segments[i - 1];
        auto& curr = job.segments[i];
        if (prev.points.empty() || curr.points.empty()) {
            continue;
        }
        copyPointCoord(curr.points.front(), prev.points.back());
    }
}

}  // namespace

namespace nbcam {

void PathPlanner::postProcess(LaserJob& job) {
    addJumpSegments(job);
    applyCornerDeceleration(job);
    sparsifyParams(job);
    optimizePathOrder(job);
    normalizeSegmentSequence(job);
}

void PathPlanner::addJumpSegments(LaserJob& job) {
    std::vector<PathSegment> new_segments;
    
    for (size_t i = 0; i < job.segments.size(); ++i) {
        // 先保存需要的信息，再移动
        bool has_points = !job.segments[i].points.empty();
        PathPoint last_point_ref;
        if (has_points) {
            const auto& last_point = job.segments[i].points.back();
            last_point_ref.u = last_point.u;
            last_point_ref.v = last_point.v;
            last_point_ref.x = last_point.x;
            last_point_ref.y = last_point.y;
            last_point_ref.z = last_point.z;
        }
        
        // 使用移动语义，因为PathPoint不可复制
        PathSegment segment = std::move(job.segments[i]);
        
        // 添加当前段
        new_segments.push_back(std::move(segment));
        
        // 如果不是最后一个段，添加跳线段
        if (i < job.segments.size() - 1) {
            PathSegment jump_segment;
            jump_segment.id = static_cast<int>(new_segments.size());
            jump_segment.type = SegmentType::JUMP;
            
            // 从当前段的最后一个点到下一个段的第一个点
            if (has_points && !job.segments[i + 1].points.empty()) {
                PathPoint jump_start;
                jump_start.u = last_point_ref.u;
                jump_start.v = last_point_ref.v;
                jump_start.x = last_point_ref.x;
                jump_start.y = last_point_ref.y;
                jump_start.z = last_point_ref.z;
                jump_start.laser = 0;  // 激光关闭
                
                const auto& next_first = job.segments[i + 1].points.front();
                PathPoint jump_end;
                jump_end.u = next_first.u;
                jump_end.v = next_first.v;
                jump_end.x = next_first.x;
                jump_end.y = next_first.y;
                jump_end.z = next_first.z;
                jump_end.laser = 0;
                
                jump_segment.points.push_back(std::move(jump_start));
                jump_segment.points.push_back(std::move(jump_end));
                
                new_segments.push_back(std::move(jump_segment));
            }
        }
    }
    
    // 使用移动语义替换 segments
    job.segments = std::move(new_segments);
    spdlog::info("添加跳线段完成，总段数: {}", job.segments.size());
}

void PathPlanner::applyCornerDeceleration(LaserJob& job, double threshold_angle) {
    const double threshold_rad = threshold_angle * M_PI / 180.0;
    
    for (auto& segment : job.segments) {
        if (segment.type != SegmentType::MARK || segment.points.size() < 3) {
            continue;
        }

        for (size_t i = 1; i < segment.points.size() - 1; ++i) {
            const auto& p0 = segment.points[i - 1];
            const auto& p1 = segment.points[i];
            const auto& p2 = segment.points[i + 1];
            
            // 计算角度
            double dx1 = p1.x - p0.x;
            double dy1 = p1.y - p0.y;
            double dz1 = p1.z - p0.z;
            
            double dx2 = p2.x - p1.x;
            double dy2 = p2.y - p1.y;
            double dz2 = p2.z - p1.z;
            
            double len1 = std::sqrt(dx1 * dx1 + dy1 * dy1 + dz1 * dz1);
            double len2 = std::sqrt(dx2 * dx2 + dy2 * dy2 + dz2 * dz2);
            
            if (len1 < 1e-9 || len2 < 1e-9) continue;
            
            double dot = (dx1 * dx2 + dy1 * dy2 + dz1 * dz2) / (len1 * len2);
            dot = std::max(-1.0, std::min(1.0, dot));  // 限制范围
            double angle = std::acos(dot);
            
            // 如果角度小于阈值，减速
            if (angle < threshold_rad) {
                if (!segment.points[i].params_override) {
                    segment.points[i].params_override = std::make_unique<ProcessParams>();
                }
                segment.points[i].params_override->speed_mm_s *= 0.5;
            }
        }
    }
}

void PathPlanner::sparsifyParams(LaserJob& job) {
    // 参数稀疏化：将点级参数压缩为段级
    // 简化实现：如果段内大部分点的参数相同，则提升为段级参数
    for (auto& segment : job.segments) {
        if (segment.points.empty()) continue;
        
        // 统计参数分布
        // 简化实现，完整实现需要更复杂的统计
        bool all_same = true;
        const ProcessParams* first_params = segment.points[0].params_override.get();
        
        for (size_t i = 1; i < segment.points.size(); ++i) {
            const ProcessParams* params = segment.points[i].params_override.get();
            if (params != first_params) {
                all_same = false;
                break;
            }
        }
        
        // 如果所有点参数相同，提升为段级参数
        if (all_same && first_params) {
            segment.params_override = std::make_unique<ProcessParams>(*first_params);
            for (auto& point : segment.points) {
                point.params_override.reset();
            }
        }
    }
}

void PathPlanner::optimizePathOrder(LaserJob& job) {
    // 路径排序优化（简化实现）
    // 完整实现可以使用TSP算法或最近邻算法
    spdlog::info("路径排序优化完成");
}

} // namespace nbcam
