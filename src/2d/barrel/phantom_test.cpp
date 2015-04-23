#include "util/test.h"

#include <cmath>

#include "phantom.h"

using namespace PET2D;
using namespace PET2D::Barrel;

TEST("2d/barrel/phantom/phantom") {

  Phantom<float> phantom;
  phantom.emplace_back(0, 1, 1, 0.5, M_PI / 3, 0.75);
  phantom.emplace_back(1, 1, 2, 2, 0, 0.5);

  SECTION("intensity") {
    REQUIRE(0.50 == phantom.intensity({ 1, 1 }));
    REQUIRE(0.50 == phantom.intensity({ 1.563, -0.8545 }));
    REQUIRE(0.00 == phantom.intensity({ -0.677, -2.5 }));

    REQUIRE(0.75 == phantom.intensity({ -0.328, 0.26 }));
    REQUIRE(0.75 == phantom.intensity({ 0.4371, 1.792 }));
  }

  SECTION("emit") {
    REQUIRE(false == /**/ phantom.test_emit({ 1, 1 }, .75));
    REQUIRE(true == /***/ phantom.test_emit({ 1, 1 }, .45));
    REQUIRE(false == /**/ phantom.test_emit({ 1.563, -0.8545 }, .51));
    REQUIRE(true == /***/ phantom.test_emit({ 1.563, -0.8545 }, .10));
    REQUIRE(false == /**/ phantom.test_emit({ -0.677, -2.5 }, .1));
    REQUIRE(false == /**/ phantom.test_emit({ -0.677, -2.5 }, .25));
    REQUIRE(false == /**/ phantom.test_emit({ -0.677, -2.5 }, 0.001));

    REQUIRE(false == /**/ phantom.test_emit({ -0.328, 0.26 }, .76));
    REQUIRE(true == /***/ phantom.test_emit({ -0.328, 0.26 }, .74));
    REQUIRE(false == /**/ phantom.test_emit({ 0.4371, 1.792 }, .77));
    REQUIRE(true == /***/ phantom.test_emit({ 0.4371, 1.792 }, .73));
  }
}
