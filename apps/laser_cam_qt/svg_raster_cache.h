#ifndef SVG_RASTER_CACHE_H
#define SVG_RASTER_CACHE_H

#include <QImage>
#include <string>

// 读取并栅格化SVG，命中缓存时避免重复解析/渲染。
bool loadSvgImageCached(const std::string& svg_filepath,
                        int width,
                        int height,
                        QImage& out_image);

#endif  // SVG_RASTER_CACHE_H
