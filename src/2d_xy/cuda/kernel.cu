#include <cuda_runtime.h>

#include <iostream>
#include <vector>
#include <cstdio>

#include <sys/time.h>

#include "config.h"
#include "prng.cuh"
#include "geometry.h"
#include "geometry_methods.cuh"
#include "detector_geometry_test.cuh"
#include "detector_hit_test.cuh"
#include "monte_carlo.cuh"

// FIXME: remove me
#include "geometry/pixel.h"
#include "2d_xy/lor.h"

using namespace gpu;

static cudaError err;

#define cuda(f, ...)                                        \
  if ((err = cuda##f(__VA_ARGS__)) != cudaSuccess) {        \
    fprintf(stderr, #f "() %s\n", cudaGetErrorString(err)); \
    exit(-1);                                               \
  }
#define cudathread_per_blockoSync(...) cuda(__VA_ARGS__)

double getwtime() {
  struct timeval tv;
  static time_t sec = 0;
  gettimeofday(&tv, NULL);
  if (!sec)
    sec = tv.tv_sec;
  return (double)(tv.tv_sec - sec) + (double)tv.tv_usec / 1e6;
}

void mem_clean_lors(MatrixElement* cpu_matrix, int number_of_blocks) {

  for (int i = 0; i < number_of_blocks; ++i) {
    for (int j = 0; j < LORS; ++j) {

      cpu_matrix[i].hit[j] = 0.f;
    }
  }
}

void run_monte_carlo_kernel(int number_of_threads_per_block,
                            int number_of_blocks,
                            int n_emissions,
                            int pixels_in_row,
                            int triangular_matrix_size,
                            float radius,
                            float h_detector,
                            float w_detector,
                            float pixel_size,
                            gpu::LOR* lookup_table_lors,
                            Pixel<>* lookup_table_pixel,
                            unsigned int* cpu_prng_seed,
                            MatrixElement* cpu_matrix,
                            MatrixElement* gpu_output) {

  dim3 blocks(number_of_blocks);
  dim3 threads(number_of_threads_per_block);

  cudaSetDevice(1);

  unsigned int* gpu_prng_seed;
  MatrixElement* gpu_MatrixElement;

  cuda(Malloc,
       (void**)&gpu_prng_seed,
       number_of_blocks * number_of_threads_per_block * 4 *
           sizeof(unsigned int));
  cuda(Malloc,
       (void**)&gpu_MatrixElement,
       number_of_blocks * sizeof(MatrixElement));

  cuda(
      Memcpy,
      gpu_prng_seed,
      cpu_prng_seed,
      number_of_blocks * number_of_threads_per_block * 4 * sizeof(unsigned int),
      cudaMemcpyHostToDevice);

  double timer = getwtime();

  float fov_radius = radius / M_SQRT2;

  for (int p = 0; p < triangular_matrix_size; ++p) {

    Pixel<> pixel = lookup_table_pixel[p];

    int i = pixel.x;
    int j = pixel.y;

    mem_clean_lors(cpu_matrix, number_of_blocks);

    cuda(Memcpy,
         gpu_MatrixElement,
         cpu_matrix,
         number_of_blocks * sizeof(MatrixElement),
         cudaMemcpyHostToDevice);

#ifdef PRINT

    long total_emissions =
        (long)n_emissions * number_of_blocks * number_of_threads_per_block;

    printf("Pixel(%d,%d) n_emissions: %d %ld\n",
           i,
           j,
           n_emissions,
           total_emissions);
#endif

    if ((i * i + j * j) * pixel_size * pixel_size < fov_radius * fov_radius) {

      monte_carlo_kernel << <blocks, threads>>> (i,
                                                 j,
                                                 n_emissions,
                                                 gpu_prng_seed,
                                                 gpu_MatrixElement,
                                                 number_of_threads_per_block,
                                                 pixels_in_row,
                                                 radius,
                                                 h_detector,
                                                 w_detector,
                                                 pixel_size);

      cudaThreadSynchronize();

      cudaError_t info = cudaGetLastError();

      if (info != cudaSuccess) {
        std::cerr << cudaGetErrorString(info) << std::endl;
      }
    }

    cuda(Memcpy,
         cpu_matrix,
         gpu_MatrixElement,
         number_of_blocks * sizeof(MatrixElement),
         cudaMemcpyDeviceToHost);

    for (int i = 0; i < LORS; i++) {
      float temp = 0.f;
      for (int j = 0; j < number_of_blocks; ++j) {

        temp += cpu_matrix[j].hit[i];
      }

      if (temp > 0.0f) {
        ::LOR<> lor(lookup_table_lors[i].lor_a, lookup_table_lors[i].lor_b);
        gpu_output[p].hit[lor.index()] = temp;

#ifdef PRINT
        printf("LOR(%d,%d) %f\n",
               lor.first,
               lor.second,
               gpu_output[p].hit[lor.index()]);
#endif
      }
    }
  }
  double time = 0.0f;

  time = getwtime() - time;

  printf("time[s]: %f\n ", time);
  printf("time per pixel: %f\n", time / triangular_matrix_size);

  cuda(Free, gpu_prng_seed);
  cuda(Free, gpu_MatrixElement);
}

#if PHANTOM_KERNEL
void phantom_kernel(int number_of_threads_per_block,
                    int number_of_blocks,
                    int n_emissions,
                    int pixels_in_row,
                    float radius,
                    float h_detector,
                    float w_detector,
                    float pixel_size,
                    Pixel<>* lookup_table_pixel,
                    Lor* lookup_table_lors,
                    std::vector<MatrixElement>& gpu_output) {

  dim3 blocks(number_of_blocks);
  dim3 threads(number_of_threads_per_block);

  unsigned int* cpu_prng_seed;
  float fov_radius = radius / M_SQRT2;
  cudaSetDevice(1);

  cpu_prng_seed =
      (unsigned int*)malloc(number_of_blocks * number_of_threads_per_block * 4 *
                            sizeof(unsigned int));

  for (int i = 0; i < 4 * number_of_blocks * number_of_threads_per_block; ++i) {

    cpu_prng_seed[i] = 53445 + i;
  }

  int triangular_matrix_size =
      ((pixels_in_row / 2) * ((pixels_in_row / 2) + 1) / 2);

  for (int i = 0; i < triangular_matrix_size; ++i) {

    for (int lor = 0; lor < LORS; ++lor) {

      gpu_output[i].hit[lor] = 0;
    }
  }

  MatrixElement* cpu_matrix =
      (MatrixElement*)malloc(number_of_blocks * sizeof(MatrixElement));

  unsigned int* gpu_prng_seed;
  MatrixElement* gpu_MatrixElement;

  cuda(Malloc,
       (void**)&gpu_prng_seed,
       number_of_blocks * number_of_threads_per_block * 4 *
           sizeof(unsigned int));
  cuda(Malloc,
       (void**)&gpu_MatrixElement,
       number_of_blocks * sizeof(MatrixElement));

  cuda(
      Memcpy,
      gpu_prng_seed,
      cpu_prng_seed,
      number_of_blocks * number_of_threads_per_block * 4 * sizeof(unsigned int),
      cudaMemcpyHostToDevice);

  printf("GPU kernel start\n");
  printf("DETECTORS %d LORS: %d\n", NUMBER_OF_DETECTORS, LORS);

  double timer = getwtime();

  for (int p = 0; p < triangular_matrix_size; ++p) {

    Pixel<> pixel = lookup_table_pixel[p];

    int i = pixel.x;
    int j = pixel.y;

    mem_clean_lors(cpu_matrix, number_of_blocks);

    cuda(Memcpy,
         gpu_MatrixElement,
         cpu_matrix,
         number_of_blocks * sizeof(MatrixElement),
         cudaMemcpyHostToDevice);

    long total_emissions =
        (long)n_emissions * number_of_blocks * number_of_threads_per_block;

    printf("Pixel(%d,%d) n_emissions: %d %ld\n",
           i,
           j,
           n_emissions,
           total_emissions);

    if ((i * i + j * j) * pixel_size * pixel_size < fov_radius * fov_radius) {
      gpu_phantom_generation << <blocks, threads>>>
          (i,
           j,
           n_emissions,
           gpu_prng_seed,
           gpu_MatrixElement,
           number_of_threads_per_block,
           pixels_in_row,
           radius,
           h_detector,
           w_detector,
           pixel_size);

      cudaThreadSynchronize();

      cudaError_t info = cudaGetLastError();

      if (info != cudaSuccess) {
        std::cerr << cudaGetErrorString(info) << std::endl;
      }
    }

    cuda(Memcpy,
         cpu_matrix,
         gpu_MatrixElement,
         number_of_blocks * sizeof(MatrixElement),
         cudaMemcpyDeviceToHost);

    if (p == 0) {
      for (int i = 0; i < LORS; i++) {
        float temp = 0.f;
        for (int j = 0; j < number_of_blocks; ++j) {

          temp += cpu_matrix[j].hit[i];
        }

        if (temp > 0.0f) {
          LOR<> lor(lookup_table_lors[i].lor_a, lookup_table_lors[i].lor_b);
          gpu_output[p].hit[lor.index()] = temp;

          //        printf("LOR(%d,%d) %f\n",
          //               lor.first,
          //               lor.second,
          //               gpu_output[p].hit[lor.index()]);
        }
      }
    }
  }
  double time = 0.0f;

  time = getwtime() - time;

  printf("time[s]: %f\n ", time);
  printf("time per pixel: %f\n", time / triangular_matrix_size);

  cuda(Free, gpu_prng_seed);
  cuda(Free, gpu_MatrixElement);
}
#endif
