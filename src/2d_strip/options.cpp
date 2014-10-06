#include "options.h"

#include "cmdline.h"
#include "util/cmdline_types.h"
#include "util/cmdline_hooks.h"
#include "util/variant.h"

void add_detector_options(cmdline::parser& cl) {
  cl.add<cmdline::path>("config",
                        'c',
                        "load config file",
                        cmdline::dontsave,
                        cmdline::path(),
                        cmdline::default_reader<cmdline::path>(),
                        cmdline::load);

  cl.add<double>(
      "r-distance", 'r', "R distance between scintillator", false, 500);
  cl.add<double>("s-length", 'l', "scintillator length", false, 1000);
  cl.add<double>("p-size", 'p', "pixel size", false, 5);
  cl.add<int>("n-pixels", 'n', "number of pixels", false, 200);
  cl.add<int>("n-z-pixels", '\0', "number of z pixels", false);
  cl.add<int>("n-y-pixels", '\0', "number of y pixels", false);
  cl.add<double>("s-z", 's', "Sigma z error", false, 10);
  cl.add<double>("s-dl", 'd', "Sigma dl error", false, 42);
}

void add_reconstruction_options(cmdline::parser& cl) {
  add_detector_options(cl);

  std::ostringstream msg;
  msg << "events_file ..." << std::endl;
  msg << "build: " << VARIANT << std::endl;
  msg << "note: All length options below should be expressed in milimeters.";
  cl.footer(msg.str());

  cl.add<int>("blocks", 'i', "number of iteration blocks", false, 0);
  cl.add<int>("iterations", 'I', "number of iterations (per block)", false, 1);
  cl.add<cmdline::path>(
      "output", 'o', "output files prefix (png)", false, "rec");

  cl.add("verbose", 'v', "print the iteration information to stdout");
#if HAVE_CUDA
  cl.add("gpu", 'G', "run on GPU (via CUDA)");
  cl.add<int>("cuda-device", 'D', "CUDA device", cmdline::dontsave, 0);
  cl.add<int>("cuda-blocks", 'B', "CUDA blocks", cmdline::dontsave, 32);
  cl.add<int>(
      "cuda-threads", 'W', "CUDA threads per block", cmdline::dontsave, 512);
#endif
#if _OPENMP
  cl.add<int>("n-threads", 'T', "number of OpenMP threads", cmdline::dontsave);
#endif
}

void add_phantom_options(cmdline::parser& cl) {
  add_detector_options(cl);

  cl.footer("phantom_description");

  cl.add<cmdline::path>(
      "output", 'o', "output events file", false, "phantom.bin");
  cl.add<int>("emissions",
              'e',
              "number of emissions",
              false,
              0,
              cmdline::default_reader<int>(),
              cmdline::not_from_file);
  cl.add("verbose", 'v', "print the phantom information to stdout");
#if _OPENMP
  cl.add<int>("n-threads", 'T', "number of OpenMP threads", cmdline::dontsave);
#endif
}
