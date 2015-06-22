#include "util/test.h"

#include "phantom_monte_carlo.h"

#include "2d/barrel/square_detector.h"
#include "2d/barrel/generic_scanner.h"
#include "2d/barrel/scanner_builder.h"
#include "common/model.h"

#include "3d/hybrid/scanner.h"

#include "3d/geometry/phantom.h"
#include "phantom_monte_carlo.h"

using F = float;
using S = short;
using RNG = std::mt19937;
using Detector = PET2D::Barrel::SquareDetector<F>;
using Scanner2D = PET2D::Barrel::GenericScanner<Detector, 8, short>;
using Scanner = PET3D::Hybrid::Scanner<Scanner2D>;
using Phantom = PET3D::Phantom<F, S, RNG>;
using Allways = Common::AlwaysAccept<F>;
using Scintillator = Common::ScintillatorAccept<F>;
using Point = PET3D::Point<F>;
using Vector = PET3D::Vector<F>;

namespace {
F strip_width = 0.005;
F strip_height = 0.019;
F strip_distance = 0.410;
F inner_radius = (strip_distance - strip_height) / 2;
F strip_length = 0.300;
}

TEST("common/phantom_monte_carlo/point_source") {

  auto scanner2d = PET2D::Barrel::ScannerBuilder<Scanner2D>::build_single_ring(
      inner_radius, 2, strip_width, strip_height);

  Scanner scanner(scanner2d, strip_length);
  scanner.set_sigmas(0.010, 0.024);

  using RNG = std::mt19937;
  RNG rng;
  std::vector<PET3D::PhantomRegion<float, RNG>*> regions;

  auto emitter =
      new PET3D::PointRegion<float, RNG, PET3D::SingleDirectionDistribution<F>>(
          1.0f,
          PET3D::SingleDirectionDistribution<float>(
              Vector::from_euler_angles(0, 2 * M_PI / 6)),
          Point(0, 0, 0));

  regions.push_back(emitter);
  PET3D::Phantom<float, short, RNG> phantom(regions);

  Scintillator scintillator(0.100);
  Common::PhantomMonteCarlo<Phantom, Scanner> monte_carlo(phantom, scanner);

  std::ofstream out_wo_error("test_output/point_source_no_errors.txt");
  monte_carlo.out_wo_error = out_wo_error;

  std::ofstream out_w_error("test_output/point_source_errors.txt");
  monte_carlo.out_w_error = out_w_error;

  std::ofstream out_exact_events("test_output/point_source_exact_events.txt");
  monte_carlo.out_exact_events = out_exact_events;

  std::ofstream out_full_response("test_output/point_source_full_response.txt");
  monte_carlo.out_full_response = out_full_response;
  monte_carlo.generate(rng, scintillator, 1000000);
}

TEST("common/phantom_monte_carlo/phantom_region") {

  auto scanner2d = PET2D::Barrel::ScannerBuilder<Scanner2D>::build_single_ring(
      inner_radius, 2, strip_width, strip_height);

  Scanner scanner(scanner2d, strip_length);
  scanner.set_sigmas(0.010, 0.024);

  using RNG = std::mt19937;
  RNG rng;
  std::vector<PET3D::PhantomRegion<float, RNG>*> regions;
  float angle = std::atan2(0.0025f, 0.400f);
  auto cylinder = new PET3D::CylinderRegion<float, RNG>(
      0.0015, 0.001, 1, PET3D::SphericalDistribution<float>(-angle, angle));
  PET3D::Matrix<float> R{ 1, 0, 0, 0, 0, 1, 0, 1, 0 };

  auto rotated_cylinder =
      new PET3D::RotatedPhantomRegion<float, RNG>(cylinder, R);
  regions.push_back(rotated_cylinder);
  PET3D::Phantom<float, short, RNG> phantom(regions);

  Scintillator scintillator(0.100);
  Common::PhantomMonteCarlo<Phantom, Scanner> monte_carlo(phantom, scanner);

  std::ofstream out_wo_error("test_output/no_errors.txt");
  monte_carlo.out_wo_error = out_wo_error;

  std::ofstream out_w_error("test_output/errors.txt");
  monte_carlo.out_w_error = out_w_error;

  std::ofstream out_exact_events("test_output/exact_events.txt");
  monte_carlo.out_exact_events = out_exact_events;

  std::ofstream out_full_response("test_output/full_response.txt");
  monte_carlo.out_full_response = out_full_response;
  monte_carlo.generate(rng, scintillator, 1000000);
}
