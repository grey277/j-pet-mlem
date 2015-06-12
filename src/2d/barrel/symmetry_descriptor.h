#pragma once

#include "2d/geometry/pixel.h"
#include "2d/geometry/pixel_grid.h"

namespace PET2D {
namespace Barrel {

enum Axis { X = 1, Y = 2, XY = 4 };

template <typename SType> class SymmetryDescriptor {
 public:
  using S = SType;
  SymmetryDescriptor(int n_detectors, int n_symmetries)
      : n_detectors(n_detectors), n_symmetries(n_symmetries) {
    detectors_ = new S[n_detectors * n_symmetries];
  }
  static const S EIGHT = 8;

  /// Symmetric detector
  S symmetric_detector(S detector, S symmetry) const {
    return detectors_[detector * EIGHT + symmetry];
  }


  // converts the pixels from the trainulra cut
  // to full pixel_grid pixels
  template <typename FType>
  static Pixel<S> full_grid_pixel(const PET2D::PixelGrid<FType, S> grid,
                                  const Pixel<S>& pixel) {
    return Pixel<S>(pixel.x + grid.n_columns / 2, pixel.y + grid.n_rows / 2);
  }

  template <typename FType>
  static Pixel<S> symmetric_pixel(const PET2D::PixelGrid<FType, S> grid,
                                  const Pixel<S>& pixel,
                                  S symmetry) {
    Pixel<S> s_pixel(pixel);
    if (symmetry & Axis::X) {
      s_pixel.y = grid.n_row - s_pixel.y - 1;
    }
    if (symmetry & Axis::Y) {
      s_pixel.y = grid.n_columns - s_pixel.x - 1;
    }
    if (symmetry & Axis::XY) {
      std::swap(s_pixel.x, s_pixel.y);
    }
    return s_pixel;
  }

  /// Returns symmetric detector on a ring of n_detectors, assuming that
  /// detector zero is on the positive X-axis (rotation=0).
  static S ring_symmetric_detector(S n_detectors, S detector, S symmetry) {
    if (symmetry & Axis::X) {
      detector = (n_detectors - detector) % n_detectors;  // x-axis
    }
    if (symmetry & Axis::Y) {
      detector =
          ((n_detectors + n_detectors / 2) - detector) % n_detectors;  // y-axis
    }
    if (symmetry & Axis::XY) {
      detector = ((n_detectors + n_detectors / 4) - detector) %
                 n_detectors;  // xy-axis
    }
    return detector;
  }

  /// Rotated symmetric detector
  ///
  /// Returns symmetric detector on a ring of n_detectors, assuming that
  /// detector zero is at the angle Pi/n_detectors (rotation = 0.5).
  static S rotated_ring_symmetric_detector(S n_detectors,
                                           S detector,
                                           S symmetry) {
    if (symmetry & Axis::X) {
      detector = (n_detectors - (detector + 1)) % n_detectors;  // x-axis
    }
    if (symmetry & Axis::Y) {
      detector = ((n_detectors + n_detectors / 2) - (detector + 1)) %
                 n_detectors;  // y-axis
    }
    if (symmetry & Axis::XY) {
      detector = ((n_detectors + n_detectors / 4) - (detector + 1)) %
                 n_detectors;  // xy-axis
    }
    return detector;
  }

  void set_symmetric_detector(S detector, S symmetry, S symmetric_detector) {
    detectors_[detector * EIGHT + symmetry] = symmetric_detector;
  }

#if !__CUDACC__
  void to_mathematica(std::ostream& m_out) const {
    auto out_delimiter = "";
    for (S i = 0; i < n_detectors; i++) {
      auto in_delimiter = "";
      m_out << out_delimiter << "{" << i << "->{";
      for (S s = 0; s < n_symmetries; s++) {
        m_out << in_delimiter << symmetric_detector(i, s);
        in_delimiter = ",";
      }
      m_out << "}}\n";
      out_delimiter = ",";
    }
  }
#endif

 private:
  const S n_detectors;
  const S n_symmetries;
  S* detectors_;
};
}
}
