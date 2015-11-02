#pragma once

#include "util/bstream.h"

#include "2d/barrel/lor.h"
#include "2d/geometry/pixel_grid.h"
#include "2d/geometry/point.h"
#include "2d/geometry/line_segment.h"

namespace PET2D {
namespace Barrel {

/// Keeps geometry information about LOR
////
/// Keeps geometry information about LOR such as pixels belonging to the LOR
/// together with their description using PixelInfo.
template <typename FType, typename SType> struct LORGeometry {
  using F = FType;
  using S = SType;
  using Pixel = PET2D::Pixel<S>;
  using LOR = PET2D::Barrel::LOR<S>;
  using LineSegment = PET2D::LineSegment<F>;

  /// Information about given Pixel relative to the LOR
  struct PixelInfo {
    Pixel pixel;  ///< pixel itself
    F t;          ///< position of projected Pixel along the LOR
    F distance;   ///< distance of Pixel to LOR
    F fill;       ///< percentage of Pixel inside the LOR
    F weight;     ///< custom weight of the Pixel eg. probability
  };

  using PixelInfoList = std::vector<PixelInfo>;

  LOR lor;
  LineSegment segment;
  F width;
  PixelInfoList pixel_infos;

  LORGeometry() = default;

  LORGeometry(const LOR& lor, const LineSegment& segment, const F width)
      : lor(lor), segment(segment), width(width) {}

  /// Construct LOR info from stream.
  LORGeometry(std::istream& in)
      : lor(in), segment(in), width(util::read<F>(in)) {
    size_t n_pixels;
    in >> n_pixels;
    if (in && n_pixels) {
      pixel_infos.resize(n_pixels);
      in.read((char*)&pixel_infos[0], sizeof(PixelInfo) * n_pixels);
    }
  }

  /// Construct LOR info from binary stream.
  LORGeometry(util::ibstream& in) : lor(in), segment(in), width(in.read<F>()) {
    size_t n_pixels;
    in >> n_pixels;
    if (in && n_pixels) {
      pixel_infos.resize(n_pixels);
      in.read((char*)&pixel_infos[0], sizeof(PixelInfo) * n_pixels);
    }
  }

  friend std::ostream& operator<<(std::ostream& out,
                                  const LORGeometry& lor_info) {
    out << lor_info.lor.first << ' ' << lor_info.lor.second << ' '
        << lor_info.segment << ' '  //
        << lor_info.width << ' '    //
        << 0;  // FIXME: unsupported serialization to text stream
    return out;
  }

  friend util::obstream& operator<<(util::obstream& out,
                                    const LORGeometry& lor_info) {
    out << lor_info.lor;
    out << lor_info.segment;
    out << lor_info.width;
    out << lor_info.pixel_infos.size();
    out << lor_info.pixel_infos;
    return out;
  }

  /// Sort Pixel infos using position along LOR.
  void sort() {
    std::sort(pixel_infos.begin(),
              pixel_infos.end(),
              [](const PixelInfo& a, const PixelInfo& b) { return a.t < b.t; });
  }

  void sort_by_index() {
    std::sort(pixel_infos.begin(),
              pixel_infos.end(),
              [](const PixelInfo& a, const PixelInfo& b) {
                return a.pixel.index() < b.pixel.index();
              });
  }

  // assumes that pixels in LOR are sorted by index !!
  void put_pixel(const Pixel& pixel, F weight) {
    PixelInfo pi;
    pi.pixel = pixel;
    auto p = std::lower_bound(pixel_infos.begin(),
                              pixel_infos.end(),
                              pi,
                              [](const PixelInfo& a, const PixelInfo& b) {
                                return a.pixel.index() < b.pixel.index();
                              });

    if (p == pixel_infos.end() || (p->pixel).index() != pixel.index()) {
      std::stringstream msg;
      msg << "pixel not found in LOR ";
      msg << lor.first << ":" << lor.second << " ";
      msg << pixel.index() << ", (" << pixel.x << "," << pixel.y << ")\n";
      for (auto i : pixel_infos) {
        msg << i.pixel.index() << " (" << i.pixel.x << "," << i.pixel.y
            << ")\n";
      }
      //throw msg.str();
      return ;
    }

    p->weight += weight;
  }

  /// Append Pixel info to the list.
  void push_back(const PixelInfo& pixel_info) {
    pixel_infos.push_back(pixel_info);
  }
};

}  // Barrel
}  // PET2D
