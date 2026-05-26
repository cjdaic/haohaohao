#include "svg_raster_cache.h"

#include "../core/pattern/nanosvg_wrapper.h"

#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace {

struct SvgRasterKey {
    std::string filepath;
    int width = 0;
    int height = 0;
    SvgRasterColorMode color_mode = SvgRasterColorMode::Color;

    bool operator==(const SvgRasterKey& other) const
    {
        return width == other.width &&
               height == other.height &&
               filepath == other.filepath &&
               color_mode == other.color_mode;
    }
};

struct SvgRasterKeyHash {
    std::size_t operator()(const SvgRasterKey& key) const
    {
        const std::size_t h1 = std::hash<std::string>{}(key.filepath);
        const std::size_t h2 = std::hash<int>{}(key.width);
        const std::size_t h3 = std::hash<int>{}(key.height);
        const std::size_t h4 = std::hash<int>{}(static_cast<int>(key.color_mode));
        return h1 ^ (h2 * 1315423911u) ^ (h3 * 2654435761u) ^ (h4 * 2246822519u);
    }
};

struct SvgRasterEntry {
    std::filesystem::file_time_type mtime{};
    bool has_mtime = false;
    QImage image;
};

std::unordered_map<SvgRasterKey, SvgRasterEntry, SvgRasterKeyHash> g_svg_raster_cache;
std::mutex g_svg_raster_cache_mutex;

bool queryFileMTime(const std::string& path,
                    std::filesystem::file_time_type& out_mtime,
                    bool& has_mtime)
{
    std::error_code ec;
    out_mtime = std::filesystem::last_write_time(path, ec);
    has_mtime = !ec;
    return has_mtime;
}

void convertImageToGrayscale(QImage& image)
{
    if (image.isNull()) {
        return;
    }

    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    for (int y = 0; y < rgba.height(); ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(rgba.scanLine(y));
        for (int x = 0; x < rgba.width(); ++x) {
            const QRgb pixel = row[x];
            const int gray = qGray(pixel);
            row[x] = qRgba(gray, gray, gray, qAlpha(pixel));
        }
    }
    image = std::move(rgba);
}

}  // namespace

bool loadSvgImageCached(const std::string& svg_filepath,
                        int width,
                        int height,
                        QImage& out_image,
                        SvgRasterColorMode color_mode)
{
    out_image = QImage();
    if (svg_filepath.empty() || width <= 0 || height <= 0) {
        return false;
    }

    SvgRasterKey key{svg_filepath, width, height, color_mode};
    std::filesystem::file_time_type mtime{};
    bool has_mtime = false;
    queryFileMTime(svg_filepath, mtime, has_mtime);

    {
        std::lock_guard<std::mutex> lock(g_svg_raster_cache_mutex);
        const auto it = g_svg_raster_cache.find(key);
        if (it != g_svg_raster_cache.end()) {
            const bool mtime_match =
                (!has_mtime && !it->second.has_mtime) ||
                (has_mtime && it->second.has_mtime && it->second.mtime == mtime);
            if (mtime_match && !it->second.image.isNull()) {
                out_image = it->second.image;
                return true;
            }
        }
    }

    nbcam::NanosvgWrapper svg_parser;
    if (!svg_parser.loadFromFile(svg_filepath)) {
        return false;
    }
    unsigned char* img_data = svg_parser.renderToImage(width, height);
    if (!img_data) {
        return false;
    }

    QImage image(img_data, width, height, QImage::Format_RGBA8888);
    image = image.copy();
    delete[] img_data;
    if (image.isNull()) {
        return false;
    }
    if (color_mode == SvgRasterColorMode::Grayscale) {
        convertImageToGrayscale(image);
    }

    {
        std::lock_guard<std::mutex> lock(g_svg_raster_cache_mutex);
        SvgRasterEntry entry;
        entry.mtime = mtime;
        entry.has_mtime = has_mtime;
        entry.image = image;
        g_svg_raster_cache[key] = std::move(entry);
    }

    out_image = std::move(image);
    return true;
}
