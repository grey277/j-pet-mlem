#include <iostream>
#include <utility>
#include <iomanip>
#include "cmdline.h"
#include "util/cmdline_types.h"
#include "config.h"
#include "data_structures.h"

#include "2d_xy/detector_ring.h"
#include "2d_xy/square_detector.h"
#include "geometry/point.h"

// reference stuff from kernel.cu file

void phantom_kernel(int number_of_threads_per_block,
                    int blocks,
                    int n_emissions,
                    int pixels_in_row,
                    float radius,
                    float h_detector,
                    float w_detector,
                    float pixel_size);

void gpu_detector_geometry_kernel_test(float radius,
                                       float h_detector,
                                       float w_detector,
                                       float pixel_size,
                                       Detector_Ring& cpu_output);

int main(int argc, char* argv[]) {

  try {

    cmdline::parser cl;
    cl.footer("matrix_file ...");

    cl.add<cmdline::string>(
        "config", 'c', "load config file", false, cmdline::string(), false);
    cl.add<int>(
        "n-pixels", 'n', "number of pixels in one dimension", false, 64);
    cl.add<int>("n-detectors", 'd', "number of ring detectors", false, 64);
    cl.add<int>("n-emissions", 'e', "emissions per pixel", false, 10000);
    cl.add<float>("radius", 'r', "inner detector ring radius", false, 100);
    cl.add<float>("s-pixel", 'p', "pixel size", false, 1.0f);
    cl.add<float>("tof-step", 'T', "TOF quantisation step", false);
    cl.add<float>("w-detector", 'w', "detector width", false, 1.0f);
    cl.add<float>("h-detector", 'h', "detector height", false, 1.0f);
    cl.add<float>("pixel-size", 's', "pixel size", false, 1.0f);
    cl.add<int>("block-num", 'B', "number of block", false, 64);
    cl.add<int>(
        "threads-per-block", 'P', "number of threads per block", false, 512);

    cl.parse_check(argc, argv);

    int pixels_in_row = cl.get<int>("n-pixels");
    int n_detectors = cl.get<int>("n-detectors");
    int n_emissions = cl.get<int>("n-emissions");
    float radius = cl.get<float>("radius");
    int s_pixel = cl.get<float>("s-pixel");
    float w_detector = cl.get<float>("w-detector");
    float h_detector = cl.get<float>("h-detector");
    // float tof_step = cl.get<float>("tof-step");

    int number_of_blocks = cl.get<int>("block-num");
    int number_of_threads_per_block = cl.get<int>("threads-per-block");

    phantom_kernel(number_of_threads_per_block,
                   number_of_blocks,
                   n_emissions,
                   pixels_in_row,
                   radius,
                   h_detector,
                   w_detector,
                   s_pixel);

    DetectorRing<> dr(n_detectors, radius, w_detector, h_detector);

    Detector_Ring cpu_output;

    typedef std::pair<int, float> error;

    std::vector<error> error_list;

    float epsilon_error = 0.0001f;

    gpu_detector_geometry_kernel_test(
        radius, h_detector, w_detector, s_pixel, cpu_output);

    for (int detector_id = 0; detector_id < NUMBER_OF_DETECTORS;
         ++detector_id) {

      auto detector_points = dr[detector_id];

#if VERBOSE
      std::cout << "DETECTOR: " << detector_id << std::endl;
#endif

      for (int point_id = 0; point_id < 4; ++point_id) {

        auto point = detector_points[point_id];
        float diff = std::fabs(
            point.x - cpu_output.detector_list[detector_id].points[point_id].x);
        if (diff > epsilon_error) {
          error_list.push_back(std::make_pair(detector_id, diff));
#if VERBOSE
          std::cout << "Diff x : " << diff << std::endl;
#endif
        }

        diff = std::fabs(
            point.y - cpu_output.detector_list[detector_id].points[point_id].y);
        if (diff > epsilon_error) {
          error_list.push_back(std::make_pair(detector_id, diff));
#if VERBOSE
          std::cout << "Diff y : " << diff << std::endl;
#endif
        }

#if VERBOSE

        std::cout << std::setprecision(10) << "Cpu representation: " << point.x
                  << " " << point.y << std::endl;
        std::cout << std::setprecision(10) << "Gpu representation: "
                  << cpu_output.detector_list[detector_id].points[point_id].x
                  << " "
                  << cpu_output.detector_list[detector_id].points[point_id].y
                  << std::endl;
#endif
      }
    }

    if (error_list.size() > 0) {

      std::cout << "Number of errors in cpu|gpu detectors geometry comparison: "
                << error_list.size() << std::endl;
    }
  }

  catch (std::string& ex) {

    std::cerr << "error: " << ex << std::endl;
  }
  catch (const char* ex) {

    std::cerr << "error: " << ex << std::endl;
  }

  return 0;
}
