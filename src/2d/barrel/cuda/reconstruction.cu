#include <cuda_runtime.h>

#include "util/cuda/debug.h"  // catches all CUDA errors
#include "util/cuda/memory.h"
#include "util/delegate.h"

#include "reconstruction.h"

namespace PET2D {
namespace Barrel {
namespace GPU {
namespace Reconstruction {

texture<float, 2, cudaReadModeElementType> tex_rho;

__global__ static void kernel(const PixelInfo* pixel_infos,
                              const size_t* lor_pixel_info_start,
                              const size_t* lor_pixel_info_end,
                              const Mean* means,
                              const int n_means,
                              float* output_rho,
                              const int width,
                              const int height,
                              const int n_blocks,
                              const int n_threads_per_block) {

  const auto tid = (blockIdx.x * blockDim.x) + threadIdx.x;

  // count output_rho[p] = y[p] -----------------------------------------------

  const auto n_threads = n_blocks * n_threads_per_block;
  const auto n_chunks = (n_means + n_threads - 1) / n_threads;

  for (int chunk = 0; chunk < n_chunks; ++chunk) {
    int mean_index = chunk * n_threads + tid;
    // check if we are still on the list
    if (mean_index >= n_means) {
      break;
    }

    auto mean = means[mean_index];
    auto lor_index = mean.lor.index();
    auto pixel_info_start = lor_pixel_info_start[lor_index];
    auto pixel_info_end = lor_pixel_info_end[lor_index];

    // count u for current lor
    F u = 0;
    for (auto i = pixel_info_start; i < pixel_info_end; ++i) {
      auto pixel_info = pixel_infos[i];
      auto pixel = pixel_info.pixel;
      u += tex2D(tex_rho, pixel.x, pixel.y) * pixel_info.weight;
    }
    F phi = mean.mean / u;
    for (auto i = pixel_info_start; i < pixel_info_end; ++i) {
      auto pixel_info = pixel_infos[i];
      auto pixel = pixel_info.pixel;
      atomicAdd(&output_rho[pixel.y * width + pixel.x],
                phi * pixel_info.weight);
    }
  }

  // count output_rho[p] *= rho[p] --------------------------------------------

  const auto n_pixels = width * height;
  const auto n_pixel_chunks = (n_pixels + n_threads - 1) / n_threads;

  for (int chunk = 0; chunk < n_pixel_chunks; ++chunk) {
    int pixel_index = chunk * n_threads + tid;
    Pixel pixel(pixel_index % width, pixel_index / width);
    // check if we are still on the list
    if (pixel_index >= n_pixels) {
      break;
    }
    // there is no collision there, so we don't need atomics
    output_rho[pixel.y * width + pixel.x] *= tex2D(tex_rho, pixel.x, pixel.y);
  }
}

void run(const SimpleGeometry& geometry,
         const Mean* means,
         int n_means,
         int width,
         int height,
         int n_iteration_blocks,
         int n_iterations_in_block,
         util::delegate<void(int iteration, float* rho)> output,
         util::delegate<void(int completed, bool finished)> progress,
         int device,
         int n_blocks,
         int n_threads_per_block,
         util::delegate<void(const char* device_name)> info) {

#if __CUDACC__
  dim3 blocks(n_blocks);
  dim3 threads(n_threads_per_block);
#endif

  cudaSetDevice(device);
  cudaDeviceProp prop;
  cudaGetDeviceProperties(&prop, device);
  info(prop.name);

  util::cuda::on_device<PixelInfo> device_pixel_infos(geometry.pixel_infos,
                                                      geometry.n_pixel_infos);
  util::cuda::on_device<size_t> device_lor_pixel_info_start(
      geometry.lor_pixel_info_start, geometry.n_lors);
  util::cuda::on_device<size_t> device_lor_pixel_info_end(
      geometry.lor_pixel_info_end, geometry.n_lors);
  util::cuda::on_device<Mean> device_means(means, n_means);

  util::cuda::memory2D<F> rho(tex_rho, width, height);
  util::cuda::on_device<F> output_rho((size_t)width * height);

  rho.set_on_device(1);

  for (int ib = 0; ib < n_iteration_blocks; ++ib) {
    for (int it = 0; it < n_iterations_in_block; ++it) {
      progress(ib * n_iterations_in_block + it, false);

      output_rho.zero_on_device();

#if __CUDACC__
#define kernel kernel<<<blocks, threads>>>
#endif
      kernel(device_pixel_infos,
             device_lor_pixel_info_start,
             device_lor_pixel_info_end,
             device_means,
             n_means,
             output_rho,
             width,
             height,
             n_blocks,
             n_threads_per_block);

      rho = output_rho;
      progress(ib * n_iterations_in_block + it, true);
    }

    rho.copy_from_device();
    output((ib + 1) * n_iterations_in_block, rho.host_ptr);
  }

  progress(n_iteration_blocks * n_iterations_in_block, false);
}

}  // Reconstruction
}  // GPU
}  // Barrel
}  // PET2D
