#pragma once

#include <vector>

#include "square_detector.h"
#include "circle_detector.h"
#include "util/svg_ostream.h"
#include "util/array.h"
#include "lor.h"
#ifndef MAX_DETECTORS
#define MAX_DETECTORS 256
#endif

#include <iostream>  // DEBUG

namespace PET2D {
namespace Barrel {

/// Detector made of several other detectors

/// No assumptions are made for how geometry of this detector looks like in
/// comparison to DetectorRing where are single detectors are placed on the
/// ring.
template <typename DetectorType = SquareDetector<double>,
          std::size_t MaxDetectors = MAX_DETECTORS,
          typename SType = int>
class DetectorSet : public util::array<MaxDetectors, DetectorType> {
 public:
  using Detector = DetectorType;
  using S = SType;
  using F = typename Detector::F;
  using LOR = Barrel::LOR<S>;
  using Pixel = PET2D::Pixel<S>;
  using Point = PET2D::Point<F>;
  using Circle = PET2D::Circle<F>;
  using Event = Barrel::Event<F>;
  using Base = util::array<MaxDetectors, Detector>;
  using CircleDetector = Barrel::CircleDetector<F>;
  using Indices = util::array<MaxDetectors, S>;
  using SideIndices = std::pair<Indices, Indices>;

  /// Makes an empty detector set.
  DetectorSet(F radius = 1, F h_detector = 1)
      : Base(),
        fov_radius(radius / M_SQRT2),
        c_inner(radius),
        c_outer(radius + h_detector) {}

  /// Makes new detector set with detectors placed on the ring of given radius.
  DetectorSet(S n_detectors,      ///< number of detectors on ring
              F radius,           ///< radius of ring
              F w_detector,       ///< width of single detector (along ring)
              F h_detector,       ///< height/depth of single detector
                                  ///< (perpendicular to ring)
              F d_detector = F()  ///< diameter of circle single detector is
                                  ///< inscribed in
              )
      : fov_radius(radius / M_SQRT2),
        c_inner(radius),
        c_outer(radius + (d_detector > F() ? d_detector : h_detector)) {
    if (radius <= 0)
      throw("invalid radius");
    if (w_detector > 0 && h_detector == 0)
      h_detector = Detector::default_height_for_width(w_detector);
    // NOTE: detector may return 0 for default height, which means we need to
    // have height given explicitely
    if (w_detector <= 0 || h_detector <= 0)
      throw("invalid detector size");
    if (n_detectors % 4)
      throw("number of detectors must be multiple of 4");

    Detector detector_base(w_detector, h_detector, d_detector);

    // move detector to the right edge of inner ring
    // along zero angle polar coordinate
    detector_base +=
        Point(0, radius + (d_detector > F() ? d_detector : h_detector) / 2);

    // fix up outer circle
    if (d_detector == F()) {
      c_outer = Circle(detector_base.max_distance());
    }

    // produce detector ring rotating base detector n times
    for (auto n = 0; n < n_detectors; ++n) {
      auto detector = detector_base;
      detector.rotate(2 * F(M_PI) * n / n_detectors - F(M_PI_2));
      this->push_back(detector);
    }
  }

  F radius() const { return c_inner.radius; }
  F outer_radius() const { return c_outer.radius; }
  F max_dl(F max_bias_size) const { return 2 * c_outer.radius + max_bias_size; }

  /// \brief Tries to detect given event.
  /// \return number of coincidences (detector hits)
  template <class RandomGenerator, class AcceptanceModel>
  _ short detect(RandomGenerator& gen,    ///< random number generator
                 AcceptanceModel& model,  ///< acceptance model
                 const Event& e,          ///< event to be detected
                 LOR& lor,                ///<[out] lor of the event
                 F& position              ///<[out] position of the event
                 ) const {
    auto indices = close_indices(e);
    S detector1, detector2;
    F depth1, depth2;
    Point d1_p1, d1_p2, d2_p1, d2_p2;
    if (!check_for_hits(
            gen, model, indices.first, e, detector1, depth1, d1_p1, d1_p2) ||
        !check_for_hits(
            gen, model, indices.second, e, detector2, depth2, d2_p1, d2_p2))
      return 0;

    lor = LOR(detector1, detector2);

    Point origin(e.x, e.y);
    F length1 = origin.nearest_distance(d1_p1, d1_p2) + depth1;
    F length2 = origin.nearest_distance(d2_p1, d2_p2) + depth2;

    if (detector1 > detector2) {
      position = length1 - length2;
    } else {
      position = length2 - length1;
    }

    return 2;
  }

  /// Quantizes position across lor
  _ static S quantize_tof_position(F position,    ///< position across lor
                                   F step_size,   ///< step size
                                   S n_positions  ///< number of positions
                                   ) {
    // number of positions if always even, lower half are negative positions
    // where 0 means position closests to detector with higher index
    // maximum means position closests to detector with lower index
    if (position < 0)
      return n_positions / 2 - 1 -
             static_cast<S>(compat::floor(-position / step_size));
    else
      return static_cast<S>(compat::floor(position / step_size)) +
             n_positions / 2;
  }

  /// Returns number of position steps (indexes)
  S n_tof_positions(F step_size,     ///< step size
                    F max_bias_size  ///< possible bias (fuzz) maximum size
                    ) const {
    // since position needs to be symmetric against (0,0) number must be even
    return (static_cast<S>(ceil(2 * max_dl(max_bias_size) / step_size)) + 1) /
           2 * 2;
  }

  /// \return indices of detectors close to given event
  SideIndices close_indices(const Event& e  ///< event to be detected
                            ) const {
    SideIndices indices;
    S distances[MaxDetectors];
    auto pe = e.perpendicular();
    // select only these crossing circle circumscribed on detector
    for (int i = 0; i < c_detectors.size(); ++i) {
      auto& circle = c_detectors[i];
      if (circle.intersects(e)) {
        auto distance = pe(circle);
        distances[i] = distance;
        if (distance < 0)
          indices.first.emplace_back(i);
        else
          indices.second.emplace_back(i);
      }
    }
    // sort them so the closest go first
    std::sort(indices.first.begin(), indices.first.end(), [&](S a, S b) {
      return distances[a] > distances[b];
    });
    std::sort(indices.second.begin(), indices.second.end(), [&](S a, S b) {
      return distances[a] < distances[b];
    });
    return indices;
  }

 private:
  template <class RandomGenerator, class AcceptanceModel>
  _ bool check_for_hits(RandomGenerator& gen,
                        AcceptanceModel& model,
                        const Indices& indices,
                        Event e,
                        S& detector,
                        F& depth,
                        Point& p1,
                        Point& p2) const {

    for (auto i : indices) {
      auto intersections = (*this)[i].intersections(e);
      // check if we got 2 point intersection
      // then test the model against these points distance
      if (intersections.size() == 2) {
        auto deposition_depth = model.deposition_depth(gen);
#if DEBUG
        std::cerr << "dep " << deposition_depth << " "
                  << (intersections[1] - intersections[0]).length()
                  << std::endl;
#endif
        if (deposition_depth < (intersections[1] - intersections[0]).length()) {
          detector = i;
          depth = deposition_depth;
          p1 = intersections[0];
          p2 = intersections[1];
          return true;
        }
      }
    }
    return false;
  }

 public:
#if !__CUDACC__
  friend util::svg_ostream<F>& operator<<(util::svg_ostream<F>& svg,
                                          DetectorSet& cd) {
    svg << cd.c_outer;
    svg << cd.c_inner;

    svg << "<g id=\"photomultipiers\">" << std::endl;
    for (auto& detector : cd.c_detectors) {
      svg << detector;
    }
    svg << "</g>" << std::endl;

    svg << "<g id=\"scintillators\">" << std::endl;
    for (auto& detector : cd) {
      svg << detector;
    }
    svg << "</g>" << std::endl;

    return svg;
  }
#endif

  void push_back(const Detector& detector) {
    Base::push_back(detector);
    c_detectors.push_back(this->back().circumscribe_circle());
  }

  template <class... Args> void emplace_back(Args&&... args) {
    Base::emplace_back(std::forward<Args>(args)...);
    c_detectors.push_back(this->back().circumscribe_circle());
  }

  const CircleDetector& circumscribed(int i) const { return c_detectors[i]; }

  const F fov_radius;

 protected:
  util::array<MaxDetectors, CircleDetector> c_detectors;
  Circle c_inner;
  Circle c_outer;
};
}
}
