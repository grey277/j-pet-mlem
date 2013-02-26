#pragma once

#include <map>

#include "random.h"
#include "detector.h"
#include "circle.h"
#include "svg_ostream.h"
#include "point.h"
#include "pixel.h"
#include "lor.h"

/// Provides model for 2D ring of detectors
template <typename FType = double, typename SType = int>
class DetectorRing : public std::vector<Detector<FType>> {
 public:
  typedef FType F;
  typedef SType S;
  typedef ::LOR<S> LOR;
  typedef ::Pixel<S> Pixel;
  typedef ::Circle<F> Circle;
  typedef ::Point<F> Point;
  typedef ::Detector<F> Detector;
  typedef ::Event<F> Event;

  /// @param n_detectors number of detectors on ring
  /// @param radius      radius of ring
  /// @param w_detector  width of single detector (along ring)
  /// @param h_detector  height/depth of single detector
  ///                    (perpendicular to ring)
  DetectorRing(S n_detectors, F radius, F w_detector, F h_detector)
      : c_inner_(radius),
        c_outer_(radius + h_detector),
        n_detectors_(n_detectors),
        n_lors_(n_detectors * (n_detectors + 1) / 2),
        radius_diff_(c_outer_.radius() - c_inner_.radius()) {
    if (radius <= 0.)
      throw("invalid radius");
    if (w_detector <= 0. || h_detector <= 0.)
      throw("invalid detector size");
    if (n_detectors_ % 4)
      throw("number of detectors must be multiple of 4");

    fov_radius_ = radius / M_SQRT2;

    Detector detector_base(h_detector, w_detector);

    // move detector to the right edge of inner ring
    // along zero angle polar coordinate
    detector_base += Point(radius + h_detector / 2, 0.);

    // produce detector ring rotating base detector n times
    for (auto n = 0; n < n_detectors_; ++n) {
      this->push_back(detector_base.rotated(2. * M_PI * n / n_detectors_));
    }
  }

  F radius() const { return c_inner_.radius(); }
  S lors() const { return n_lors_; }
  S detectors() const { return n_detectors_; }
  F fov_radius() const { return fov_radius_; }

  Pixel pixel(F x, F y, F pixel_size) {
    F rx = x + fov_radius();
    F ry = y + fov_radius();
    return Pixel(static_cast<S>(floor(rx / pixel_size)),
                 static_cast<S>(floor(ry / pixel_size)));
  }

  /// Quantizes position with given:
  /// @param step_size      step size
  /// @param max_bias_size  possible bias (fuzz) maximum size
  S quantize_position(F position, F step_size, F max_bias_size) {
    // FIXME: rounding?
    return static_cast<S>((position - radius_diff_ - max_bias_size) /
                          step_size);
  }

  /// Returns number of position steps (indexes) for:
  /// @param step_size      step size
  /// @param max_bias_size  possible bias (fuzz) maximum size
  S n_positions(F step_size, F max_bias_size) {
    return static_cast<S>(
        ceil(2.0 * (radius_diff_ + max_bias_size) / step_size)) + 1;
  }

  template <class RandomGenerator, class AcceptanceModel>
  bool check_for_hits(RandomGenerator& gen,
                      AcceptanceModel& model,
                      S inner,
                      S outer,
                      Event e,
                      S& detector,
                      F& depth) {

    // tells in which direction we got shorter modulo distance
    S step = ((n_detectors_ + inner - outer) % n_detectors_ >
              (n_detectors_ + outer - inner) % n_detectors_)
             ? 1
             : n_detectors_ - 1;
    S end = (outer + step) % n_detectors_;
    for (auto i = inner; i != end; i = (i + step) % n_detectors_) {
      auto points = (*this)[i].intersections(e);
      // check if we got 2 point intersection
      // then test the model against these points distance
      if (points.size() == 2) {
        auto deposition_depth = model.deposition_depth(gen);
        if (deposition_depth < (points[1] - points[0]).length()) {
          detector = i;
          depth = deposition_depth;
          return true;
        }
      }
    }
    return false;
  }

  /// @param model acceptance model
  /// @param rx, ry coordinates of the emission point
  /// @param output parameter contains the lor of the event
  template <class RandomGenerator, class AcceptanceModel>
  short emit_event(RandomGenerator& gen,
                   AcceptanceModel& model,
                   F rx,
                   F ry,
                   F angle,
                   LOR& lor,
                   F& position) {

    typename Circle::Event e(rx, ry, angle);

    auto inner_secant = c_inner_.secant(e);
    auto outer_secant = c_outer_.secant(e);

    auto i_inner =
        c_inner_.section(c_inner_.angle(inner_secant.first), n_detectors_);
    auto i_outer =
        c_outer_.section(c_inner_.angle(outer_secant.first), n_detectors_);
    S detector1;
    F depth1;
    if (!check_for_hits(gen, model, i_inner, i_outer, e, detector1, depth1))
      return 0;

    i_inner =
        c_inner_.section(c_inner_.angle(inner_secant.second), n_detectors_);
    i_outer =
        c_outer_.section(c_inner_.angle(outer_secant.second), n_detectors_);
    S detector2;
    F depth2;
    if (!check_for_hits(gen, model, i_inner, i_outer, e, detector2, depth2))
      return 0;

    lor.first = detector1;
    lor.second = detector2;
    position = depth1 - depth2;
    return 2;
  }

  friend svg_ostream<F>& operator<<(svg_ostream<F>& svg, DetectorRing& dr) {
    svg << dr.c_outer_;
    svg << dr.c_inner_;

    for (auto detector = dr.begin(); detector != dr.end(); ++detector) {
      svg << *detector;
    }

    return svg;
  }

 private:
  Circle c_inner_;
  Circle c_outer_;
  S n_detectors_;
  S n_lors_;
  F fov_radius_;
  F radius_diff_;
};
