#pragma once

#include "2d/geometry/point.h"
#include "util/cuda/compat.h"

namespace PET2D {
namespace Barrel {

/// Model for 2D barrel PET event
template <typename FType = double> struct Event : public PET2D::Point<FType> {
  using F = FType;
  using Point = PET2D::Point<F>;
  using Base = Point;

  Event() = delete;

  _ Event(F x, F y, F phi)
      : Event(x,
              y,
              phi,
              // line equation a b coefficients: a x + b y == c
              std::sin(phi),
              -std::cos(phi)) {}

 private:
  _ Event(F x, F y, F phi, F a, F b)
      : Base(x, y),
        phi(phi),
        a(a),
        b(b),
        // line equation c coefficient: a x + b y == c
        c(a * x + b * y),
        // helper variables
        b2c(b * b * c),
        ac(a * c),
        c2(c * c),
        inv_b(1 / b) {}

 public:
  _ Event(Base p, F phi) : Event(p.x, p.y, phi) {}

  // evaluates line equation side on given point
  // 0 means points lies on the line, -1 left, 1 right
  _ F operator()(const Point& p) const { return a * p.x + b * p.y - c; }

  /// \returns perpendicular event line
  _ Event perpendicular() const {
    return Event(this->x, this->y, phi + M_PI_2, -b, a);
  }

  _ Event operator+(const Point& p) const {
    return Event(this->x + p.x, this->y + p.y, phi, a, b);
  }

  _ Event operator-(const Point& p) const {
    return Event(this->x - p.x, this->y - p.y, phi, a, b);
  }

  const F phi;

  // line equation coefficients
  const F a, b, c;
  // precalculated variables
  const F b2c, ac, c2, inv_b;
};
}  // Barrel
}  // PET2D
