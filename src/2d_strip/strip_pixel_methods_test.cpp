#include "catch.hpp"
#include <vector>
#include <cmath>

#include "util/png_writer.h"
#include "util/bstream.h"
#include "util/svg_ostream.h"

#include "strip_detector.h"

TEST_CASE("strip/pixel/location") {

  StripDetector<> detector(500, 1000, 200, 200, 5, 5, 10, 63);

  CHECK(detector.pixel_location(Point<>(0.0, 0.0)) == Pixel<>(100, 100));

  // test boundary points
  CHECK(detector.pixel_location(Point<>(500.0, -500.0)) == Pixel<>(0, 0));
  CHECK(detector.pixel_location(Point<>(500.0, 500.0)) == Pixel<>(0, 200));
  CHECK(detector.pixel_location(Point<>(-500.0, -500.0)) == Pixel<>(200, 0));
  CHECK(detector.pixel_location(Point<>(-500.0, 500.0)) == Pixel<>(200, 200));
}

TEST_CASE("strip/pixel/center") {

  // space->image_space  y: [R,-R] ->[0,n_pixels_y], z:[-L/2,L/2] ->
  // [0,n_pixels_z]
  StripDetector<> detector(500, 1000, 200, 200, 5, 5, 10, 63);

  // test middle point pixel center
  CHECK(detector.pixel_center(detector.pixel_location(Point<>(0.0, 0.0))) ==
        Point<>(-2.5, 2.5));
  // test -y,+z
  CHECK(detector.pixel_center(detector.pixel_location(Point<>(-6.0, 3.0))) ==
        Point<>(-7.5, 2.5));
  // test +y,+z
  CHECK(detector.pixel_center(detector.pixel_location(Point<>(6.0, 3.0))) ==
        Point<>(7.5, 2.5));
  // test +y,-z
  CHECK(detector.pixel_center(detector.pixel_location(Point<>(6.0, -3.0))) ==
        Point<>(7.5, -2.5));
  // test -y,-z
  CHECK(detector.pixel_center(detector.pixel_location(Point<>(-6.0, -3.0))) ==
        Point<>(-7.5, -2.5));
}
