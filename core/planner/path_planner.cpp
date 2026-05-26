#include "path_planner.h"
#include <spdlog/spdlog.h>
#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

constexpr int kGrayscaleBucketCount = 26;
constexpr int kBoundaryBucket = kGrayscaleBucketCount;

int grayscaleToBucket(double gray01)
{
    const int gray256 = static_cast<int>(std::floor(std::clamp(gray01, 0.0, 1.0) * 256.0));
    return std::clamp(gray256 / 10, 0, kGrayscaleBucketCount - 1);
}

double bucketToGray01(int bucket)
{
    if (bucket >= kBoundaryBucket) {
        return 1.0;
    }
    return std::clamp(static_cast<double>(bucket) / 25.0, 0.0, 1.0);
}

bool sameProcessParams(const nbcam::ProcessParams& a, const nbcam::ProcessParams& b)
{
    return std::abs(a.power_w - b.power_w) <= 1e-6 &&
           std::abs(a.freq_hz - b.freq_hz) <= 1e-6 &&
           std::abs(a.speed_mm_s - b.speed_mm_s) <= 1e-6 &&
           a.laser_on_delay_us == b.laser_on_delay_us &&
           a.laser_off_delay_us == b.laser_off_delay_us &&
           std::abs(a.grayscale - b.grayscale) <= 1e-6;
}

void copyPointCoord(nbcam::PathPoint& dst, const nbcam::PathPoint& src)
{
    dst.u = src.u;
    dst.v = src.v;
    dst.x = src.x;
    dst.y = src.y;
    dst.z = src.z;
    dst.a = src.a;
    dst.b = src.b;
    dst.grayscale = src.grayscale;
}

void normalizeSegmentSequence(nbcam::LaserJob& job)
{
    job.segments.erase(
        std::remove_if(job.segments.begin(), job.segments.end(),
                       [](const nbcam::PathSegment& seg) { return seg.points.size() < 2; }),
        job.segments.end());

    for (size_t i = 0; i < job.segments.size(); ++i) {
        job.segments[i].id = static_cast<int>(i);
    }
}

int segmentSortBucket(const nbcam::PathSegment& segment)
{
    if (segment.svg_boundary) {
        return kBoundaryBucket;
    }
    if (segment.grayscale_bucket >= 0) {
        return std::clamp(segment.grayscale_bucket, 0, kGrayscaleBucketCount - 1);
    }
    if (segment.points.empty()) {
        return 0;
    }

    double sum = 0.0;
    size_t valid = 0;
    for (const auto& point : segment.points) {
        if (std::isfinite(point.grayscale)) {
            sum += std::clamp(point.grayscale, 0.0, 1.0);
            ++valid;
        }
    }
    return grayscaleToBucket(valid > 0 ? sum / static_cast<double>(valid) : 0.0);
}

}  // namespace

namespace nbcam {

void PathPlanner::postProcess(LaserJob& job)
{
    applyCornerDeceleration(job);
    assignGrayscaleBuckets(job,
                           job.grayscale_power_min_w,
                           job.grayscale_power_max_w,
                           job.grayscale_gamma);
    sparsifyParams(job);
    optimizePathOrder(job);
    addJumpSegments(job);
    normalizeSegmentSequence(job);
}

void PathPlanner::addJumpSegments(LaserJob& job)
{
    std::vector<PathSegment> new_segments;
    new_segments.reserve(job.segments.size() * 2);

    for (size_t i = 0; i < job.segments.size(); ++i) {
        bool has_points = !job.segments[i].points.empty();
        PathPoint last_point_ref;
        if (has_points) {
            copyPointCoord(last_point_ref, job.segments[i].points.back());
        }

        PathSegment segment = std::move(job.segments[i]);
        new_segments.push_back(std::move(segment));

        if (i < job.segments.size() - 1) {
            PathSegment jump_segment;
            jump_segment.id = static_cast<int>(new_segments.size());
            jump_segment.type = SegmentType::JUMP;

            if (has_points && !job.segments[i + 1].points.empty()) {
                PathPoint jump_start;
                copyPointCoord(jump_start, last_point_ref);
                jump_start.laser = 0;

                const auto& next_first = job.segments[i + 1].points.front();
                PathPoint jump_end;
                copyPointCoord(jump_end, next_first);
                jump_end.laser = 0;

                jump_segment.points.push_back(std::move(jump_start));
                jump_segment.points.push_back(std::move(jump_end));

                new_segments.push_back(std::move(jump_segment));
            }
        }
    }

    job.segments = std::move(new_segments);
    spdlog::info("添加跳线段完成，总段数: {}", job.segments.size());
}

void PathPlanner::applyCornerDeceleration(LaserJob& job, double threshold_angle)
{
    const double threshold_rad = threshold_angle * M_PI / 180.0;

    for (auto& segment : job.segments) {
        if (segment.type != SegmentType::MARK || segment.points.size() < 3) {
            continue;
        }

        for (size_t i = 1; i < segment.points.size() - 1; ++i) {
            const auto& p0 = segment.points[i - 1];
            const auto& p1 = segment.points[i];
            const auto& p2 = segment.points[i + 1];

            const double dx1 = p1.x - p0.x;
            const double dy1 = p1.y - p0.y;
            const double dz1 = p1.z - p0.z;
            const double dx2 = p2.x - p1.x;
            const double dy2 = p2.y - p1.y;
            const double dz2 = p2.z - p1.z;

            const double len1 = std::sqrt(dx1 * dx1 + dy1 * dy1 + dz1 * dz1);
            const double len2 = std::sqrt(dx2 * dx2 + dy2 * dy2 + dz2 * dz2);
            if (len1 < 1e-9 || len2 < 1e-9) {
                continue;
            }

            double dot = (dx1 * dx2 + dy1 * dy2 + dz1 * dz2) / (len1 * len2);
            dot = std::clamp(dot, -1.0, 1.0);
            const double angle = std::acos(dot);
            if (angle < threshold_rad) {
                if (!segment.points[i].params_override) {
                    segment.points[i].params_override = std::make_unique<ProcessParams>();
                }
                segment.points[i].params_override->speed_mm_s *= 0.5;
            }
        }
    }
}

void PathPlanner::sparsifyParams(LaserJob& job)
{
    for (auto& segment : job.segments) {
        if (segment.points.empty()) {
            continue;
        }

        const ProcessParams* first_params = segment.points.front().params_override.get();
        if (!first_params) {
            continue;
        }

        bool all_same = true;
        for (size_t i = 1; i < segment.points.size(); ++i) {
            const ProcessParams* params = segment.points[i].params_override.get();
            if (!params || !sameProcessParams(*first_params, *params)) {
                all_same = false;
                break;
            }
        }

        if (all_same) {
            segment.params_override = std::make_unique<ProcessParams>(*first_params);
            for (auto& point : segment.points) {
                point.params_override.reset();
            }
        }
    }
}

void PathPlanner::optimizePathOrder(LaserJob& job)
{
    if (!job.grayscale_enabled) {
        spdlog::info("路径排序优化跳过: 未启用灰度分区");
        return;
    }

    for (size_t i = 0; i < job.segments.size(); ++i) {
        job.segments[i].id = static_cast<int>(i);
    }

    std::stable_sort(job.segments.begin(), job.segments.end(),
                     [](const PathSegment& a, const PathSegment& b) {
                         const int a_bucket = segmentSortBucket(a);
                         const int b_bucket = segmentSortBucket(b);
                         if (a_bucket != b_bucket) {
                             return a_bucket < b_bucket;
                         }
                         return a.id < b.id;
                     });

    spdlog::info("路径排序优化完成: 灰度分区低->高，SVG边界最后");
}

double PathPlanner::bucketToPower(int bucket, double min_power_w, double max_power_w, double gamma)
{
    const double lo = std::min(min_power_w, max_power_w);
    const double hi = std::max(min_power_w, max_power_w);
    const double g = (std::isfinite(gamma) && gamma > 1e-9) ? gamma : 1.0;
    const double gray = bucketToGray01(bucket);
    const double shaped = std::pow(gray, g);
    return lo + shaped * (hi - lo);
}

void PathPlanner::assignGrayscaleBuckets(LaserJob& job, double min_power_w, double max_power_w, double gamma)
{
    if (!job.grayscale_enabled) {
        return;
    }

    const double lo = std::min(min_power_w, max_power_w);
    const double hi = std::max(min_power_w, max_power_w);
    job.grayscale_power_min_w = lo;
    job.grayscale_power_max_w = hi;
    job.grayscale_gamma = (std::isfinite(gamma) && gamma > 1e-9) ? gamma : 1.0;

    for (auto& segment : job.segments) {
        if (segment.type != SegmentType::MARK || segment.points.empty()) {
            continue;
        }

        if (segment.svg_boundary) {
            segment.grayscale_bucket = kBoundaryBucket;
            for (auto& point : segment.points) {
                point.grayscale = 1.0;
            }
        } else if (segment.grayscale_bucket < 0) {
            segment.grayscale_bucket = segmentSortBucket(segment);
        }

        const double gray01 = (segment.grayscale_bucket >= kBoundaryBucket)
                                  ? 1.0
                                  : bucketToGray01(segment.grayscale_bucket);
        const double power_w = bucketToPower(segment.grayscale_bucket, lo, hi, job.grayscale_gamma);

        ProcessParams params = segment.params_override ? *segment.params_override : job.process_defaults;
        params.power_w = power_w;
        params.grayscale = gray01;
        segment.params_override = std::make_unique<ProcessParams>(params);

        for (auto& point : segment.points) {
            point.grayscale = segment.svg_boundary ? 1.0 : std::clamp(point.grayscale, 0.0, 1.0);
            if (point.params_override) {
                point.params_override->power_w = power_w;
                point.params_override->grayscale = gray01;
            }
        }
    }

    spdlog::info("灰度变功率分区完成: P=[{:.3f},{:.3f}]W, gamma={:.3f}, bucket=10灰度/区",
                 lo,
                 hi,
                 job.grayscale_gamma);
}

}  // namespace nbcam
