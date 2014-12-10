#pragma once

#include "cmdline.h"

namespace PET2D {
namespace Strip {

/// adds detector specific command line options
void add_detector_options(cmdline::parser& parser);

/// adds reconstruction command specific command line options
void add_reconstruction_options(cmdline::parser& parser);

/// adds phantom command specific command line options
void add_phantom_options(cmdline::parser& parser);

/// calculates all empty values from existing other parameters
void calculate_detector_options(cmdline::parser& parser);

/// provides initialization list for creating detector
#define __PET2D_STRIP(...) __VA_ARGS__  // just pass-through
#define PET2D_STRIP_DETECTOR_CL(cl)           \
  __PET2D_STRIP(cl.get<double>("r-distance"), \
                cl.get<double>("s-length"),   \
                cl.get<int>("n-y-pixels"),    \
                cl.get<int>("n-z-pixels"),    \
                cl.get<double>("p-size"),     \
                cl.get<double>("p-size"),     \
                cl.get<double>("s-z"),        \
                cl.get<double>("s-dl"))

}  // Strip
}  // PET2D
