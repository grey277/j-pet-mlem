/// \page cmd_3d_hybrid_reconstruction 3d_hybrid_reconstruction
/// \brief 3D Longitudinal PET reconstruction tool
///
/// Reconstructs image using given system matrix produced by \ref
/// cmd_3d_hybrid_matrix and mean file representing physical detector response
/// or simulated response output from \ref cmd_3d_hybrid_phantom.
///
/// Usage
/// -----
/// \verboutput 3d_hybrid_reconstruction
///
/// \sa \ref cmd_3d_hybrid_matrix, \ref cmd_3d_hybrid_phantom,
///     \ref cmd_3d_tool_psf

#include <iostream>
#include <fstream>

#include "cmdline.h"

#include "util/cmdline_types.h"
#include "util/cmdline_hooks.h"
#include "util/bstream.h"
#include "util/backtrace.h"
#include "util/png_writer.h"
#include "util/nrrd_writer.h"
#include "util/progress.h"

#include "2d/barrel/generic_scanner.h"
#include "2d/barrel/scanner_builder.h"
#include "2d/barrel/lor_geometry.h"
#include "2d/barrel/sparse_matrix.h"
#include "2d/strip/gaussian_kernel.h"

#include "scanner.h"
#include "reconstruction.h"
#include "options.h"

#include "common/types.h"

#if _OPENMP
#include <omp.h>
#else
#define omp_get_max_threads() 1
#define omp_get_thread_num() 0
#endif

#if HAVE_CUDA
#include "cuda/reconstruction.h"
#endif

using RNG = std::mt19937;
using Detector = PET2D::Barrel::SquareDetector<F>;
using Scanner2D = PET2D::Barrel::GenericScanner<Detector, S>;
using Scanner = PET3D::Hybrid::Scanner<Scanner2D>;
using Point = PET2D::Point<F>;
using Geometry = PET2D::Barrel::Geometry<F, S>;
using MathematicaGraphics = Common::MathematicaGraphics<F>;
using Kernel = PET2D::Strip::GaussianKernel<F>;
using Reconstruction = PET3D::Hybrid::Reconstruction<Scanner, Kernel>;
using Output = Reconstruction::Output;

int main(int argc, char* argv[]) {
  CMDLINE_TRY

  cmdline::parser cl;
  PET3D::Hybrid::add_reconstruction_options(cl);
  cl.parse_check(argc, argv);

  util::ibstream in_geometry(cl.get<std::string>("geometry"));
  Geometry geometry(in_geometry);
  if (!cl.exist("n-pixels")) {
    cl.get<int>("n-pixels") =
        std::max(geometry.grid.n_columns, geometry.grid.n_rows);
  }
  PET3D::Hybrid::calculate_resonstruction_options(cl, argc);

  auto verbose = cl.count("verbose");

  Scanner scanner(
      PET2D::Barrel::ScannerBuilder<Scanner2D>::build_multiple_rings(
          PET3D_LONGITUDINAL_SCANNER_CL(cl, F)),
      F(cl.get<double>("length")));
  scanner.set_sigmas(cl.get<double>("s-z"), cl.get<double>("s-dl"));

#if _OPENMP
  if (cl.exist("n-threads")) {
    omp_set_num_threads(cl.get<int>("n-threads"));
  }
#endif

  if (geometry.n_detectors != (int)scanner.barrel.size()) {
    throw("n_detectors mismatch");
  }

  if (verbose) {
    std::cout << "3D hybrid reconstruction:" << std::endl
              << "    detectors = " << geometry.n_detectors << std::endl;
    std::cerr << "   pixel grid = "  // grid size:
              << geometry.grid.n_columns << " x " << geometry.grid.n_rows
              << " / " << geometry.grid.pixel_size << std::endl;
  }

  Reconstruction::Grid grid(
      geometry.grid, cl.get<double>("z-left"), cl.get<int>("n-planes"));
  std::vector<std::string> matrices_fns;

  if (cl.exist("system")) {
    std::istringstream ss(cl.get<cmdline::path>("system"));
    std::string fn;
    while (std::getline(ss, fn, ',')) {
      matrices_fns.push_back(fn);
    }
    if (matrices_fns.size() != 1 && matrices_fns.size() != (size_t)(grid.n_planes / 2)) {
      throw("you must specify either one or half no. planes matrices");
    }
  }

  Reconstruction::Geometry geometry_soa(geometry, matrices_fns.size() ?: 1);

  for (size_t plane = 0; plane < geometry_soa.n_planes_half; ++plane) {
    const auto fn = matrices_fns[plane];
    if (verbose) {
      std::cerr << "system matrix = " << fn << std::endl;
    }
    geometry_soa.load_weights_from_matrix_file<Hit>(fn, plane);
  }

  Reconstruction reconstruction(scanner, grid, geometry_soa);

  auto start_iteration = 0;
  if (cl.exist("rho")) {
    auto rho_name = cl.get<cmdline::path>("rho");
    auto rho_base_name = rho_name.wo_ext();
    auto rho_ext = rho_name.ext();
    auto rho_txt = rho_ext == ".txt";
    if (rho_txt) {
      std::ifstream txt(rho_name);
      txt >> reconstruction.rho;
    } else {
      util::ibstream bin(rho_name);
      bin >> reconstruction.rho;
    }
    start_iteration = rho_base_name.scan_index();
    if (start_iteration && verbose) {
      std::cerr << "restarting at = " << start_iteration << std::endl;
    }
  }

  auto output_name = cl.get<cmdline::path>("output");
  auto output_base_name = output_name.wo_ext();
  auto output_ext = output_name.ext();
  auto output_txt = output_ext == ".txt";

  if (output_base_name.length()) {
    std::ofstream out_graphics(output_base_name + ".m");
    MathematicaGraphics graphics(out_graphics);
    graphics.add(scanner.barrel);
  }

  if (verbose) {
    std::cerr << "   voxel grid = "  // grid size:
              << reconstruction.grid.pixel_grid.n_columns << " x "
              << reconstruction.grid.pixel_grid.n_columns << " x "
              << reconstruction.grid.n_planes << std::endl;
  }

  if (!cl.exist("system")) {
    reconstruction.calculate_weight();
  }
  if (cl.exist("sens-to-one"))
    reconstruction.set_sensitivity_to_one();
  else
    reconstruction.calculate_sensitivity();

  reconstruction.normalize_geometry_weights();

  for (const auto& fn : cl.rest()) {
    if (verbose) {
      std::cerr << "     response = " << fn << std::endl;
    }
#if USE_FAST_TEXT_PARSER
    reconstruction.load_events(fn.c_str());
#else
    std::ifstream in_response(fn);
    reconstruction << in_response;
#endif
  }

  // print input events statistics
  if (verbose) {
    Reconstruction::EventStatistics st;
    reconstruction.event_statistics(st);
    std::cerr << "       events = " << reconstruction.n_events() << std::endl;
    std::cerr
        // event pixels ranges:
        << "  pixels: "
        << "min = " << st.min_pixels << ", "
        << "max = " << st.max_pixels << ", "
        << "avg = " << st.avg_pixels << std::endl
        // event planes ranges:
        << "  planes: "
        << "min = " << st.min_planes << ", "
        << "max = " << st.max_planes << ", "
        << "avg = " << st.avg_planes << std::endl
        // event voxels ranges:
        << "  voxels: "
        << "min = " << st.min_voxels << ", "
        << "max = " << st.max_voxels << ", "
        << "avg = " << st.avg_voxels << std::endl;
  }

  auto n_blocks = cl.get<int>("blocks");
  auto n_iterations_in_block = cl.get<int>("iterations");
  auto n_iterations = n_blocks * n_iterations_in_block;

  auto crop_pixels = cl.get<int>("crop");
  auto crop_origin = PET3D::Voxel<S>(
      cl.get<int>("crop-x"), cl.get<int>("crop-y"), cl.get<int>("crop-z"));
  Output cropped_output(crop_pixels, crop_pixels, crop_pixels);

  // graph Mathamatica drawing for reconstruction & naive reconstruction
  if (output_base_name.length()) {
    // reconstruction.graph_frame_event(graphics, 0);
    util::obstream out_naive(output_name);
    util::nrrd_writer nrrd_naive(
        output_base_name + ".nrrd", output_name, output_txt);
    auto image = reconstruction.naive();
    out_naive << image;
    nrrd_naive << image;
  }

  util::progress progress(verbose, n_iterations, 1, start_iteration);

#if HAVE_CUDA
  if (cl.exist("gpu")) {
    PET3D::Hybrid::GPU::Reconstruction::run(
        geometry_soa,
        reconstruction.sensitivity(),
        reconstruction.events().data(),
        reconstruction.n_events(),
        reconstruction.scanner.sigma_z(),
        reconstruction.scanner.sigma_dl(),
        reconstruction.grid,
        scanner.length,
        n_blocks,
        n_iterations_in_block,
        reconstruction.rho,
        start_iteration,
        [&](int iteration, const Output& output) {
          if (!output_base_name.length())
            return;
          auto fn = iteration >= 0
                        ? output_base_name.add_index(iteration, n_iterations)
                        : output_base_name + "_sensitivity";
          util::nrrd_writer nrrd(fn + ".nrrd", fn + output_ext, output_txt);
          bool crop = crop_pixels > 0;
          if (crop) {
            cropped_output.copy(output, crop_origin);
          }
          nrrd << (crop ? cropped_output : output);
          if (output_txt) {
            std::ofstream txt(fn + ".txt");
            txt << (crop ? cropped_output : output);
          } else {
            util::obstream bin(fn + output_ext);
            bin << (crop ? cropped_output : output);
          }
        },
        [&](int completed, bool finished) { progress(completed, finished); },
        cl.get<int>("cuda-device"),
        cl.get<int>("cuda-blocks"),
        cl.get<int>("cuda-threads"),
        [=](const char* device_name) {
          if (verbose) {
            std::cerr << "  CUDA device = " << device_name << std::endl;
          }
        });
  } else
#endif
  {
    for (int block = 0; block < n_blocks; ++block) {
      for (int i = 0; i < n_iterations_in_block; i++) {
        const auto iteration = block * n_iterations_in_block + i;
        if (iteration < start_iteration)
          continue;
        progress(iteration);
        reconstruction();
        progress(iteration, true);
      }
      const auto iteration = (block + 1) * n_iterations_in_block;
      if (iteration <= start_iteration)
        continue;
      if (!output_base_name.length())
        continue;
      auto fn = output_base_name.add_index(iteration, n_iterations);
      util::nrrd_writer nrrd(fn + ".nrrd", fn + output_ext, output_txt);
      nrrd << reconstruction.rho;
      if (output_txt) {
        std::ofstream txt(fn + ".txt");
        txt << reconstruction.rho;
      } else {
        util::obstream bin(fn + output_ext);
        bin << reconstruction.rho;
      }
    }

    // final reconstruction statistics
    const auto st = reconstruction.statistics();
    std::cerr << "  event count = " << st.used_events << std::endl;
    std::cerr << "  voxel count = " << st.used_voxels << "("
              << (double)st.used_voxels / st.used_events << " / event)"
              << std::endl;
    std::cerr << "  pixel count = " << st.used_pixels << "("
              << (double)st.used_pixels / st.used_events << " / event)"
              << std::endl;
  }

  CMDLINE_CATCH
}
