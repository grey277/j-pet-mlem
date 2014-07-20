#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>

#include "util/cuda/debug.h"  // catches all CUDA errors
#include "../event.h"

#include "config.h"

#if USE_SENSITIVITY
texture<float, 2, cudaReadModeElementType> tex_sensitivity;
#endif
texture<float, 2, cudaReadModeElementType> tex_rho;

#if THREAD_GRANULARITY
#include "reconstruction_thread_granularity.cuh"
#elif WARP_GRANULARITY
#include "reconstruction_warp_granularity.cuh"
#else
#include "reconstruction_simple.cuh"
#endif

template <typename F>
void run_gpu_reconstruction(StripDetector<F>& detector,
                            Event<F>* events,
                            int n_events,
                            int n_iteration_blocks,
                            int n_iterations_in_block,
                            void (*output_callback)(StripDetector<F>& detector,
                                                    int iteration,
                                                    F* image,
                                                    void* context),
                            void (*progress_callback)(int iteration,
                                                      void* context),
                            void* context,
                            int device,
                            int n_blocks,
                            int n_threads_per_block,
                            bool verbose) {

  cudaSetDevice(device);

#if __CUDACC__
  dim3 blocks(n_blocks);
  dim3 threads(n_threads_per_block);
#endif

  size_t image_size = detector.total_n_pixels * sizeof(F);
  size_t events_size = n_events * sizeof(F);

  const int width = detector.n_z_pixels;
  const int height = detector.n_y_pixels;

#if USE_SENSITIVITY
  F* cpu_sensitivity = (F*)malloc(image_size);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      Point<F> point = detector.pixel_center(Pixel<>(x, y));
      cpu_sensitivity[y * width + x] = detector.sensitivity(point);
    }
  }

  output_callback(detector, -1, cpu_sensitivity, context);
#endif

  F* cpu_rho = (F*)malloc(image_size);

  for (int i = 0; i < detector.total_n_pixels; ++i) {
    cpu_rho[i] = 100;
  }

  F* cpu_events_z_u = (F*)malloc(events_size);
  F* cpu_events_z_d = (F*)malloc(events_size);
  F* cpu_events_dl = (F*)malloc(events_size);

  for (int i = 0; i < n_events; ++i) {
    cpu_events_z_u[i] = events[i].z_u;
    cpu_events_z_d[i] = events[i].z_d;
    cpu_events_dl[i] = events[i].dl;
  }

  cudaChannelFormatDesc desc = cudaCreateChannelDesc<float>();

#if USE_SENSITIVITY
  F* gpu_sensitivity;
  size_t pitch_sensitivity;
  cudaMallocPitch(
      &gpu_sensitivity, &pitch_sensitivity, sizeof(F) * width, height);
  cudaMemcpy2D(gpu_sensitivity,
               pitch_sensitivity,
               cpu_sensitivity,
               sizeof(F) * width,
               sizeof(F) * width,
               height,
               cudaMemcpyHostToDevice);
  free(cpu_sensitivity);
  cudaBindTexture2D(NULL,
                    &tex_sensitivity,
                    gpu_sensitivity,
                    &desc,
                    width,
                    height,
                    pitch_sensitivity);
#endif

  F* gpu_rho;
  size_t pitch_rho;
  cudaMallocPitch(&gpu_rho, &pitch_rho, sizeof(F) * width, height);
  cudaBindTexture2D(NULL, &tex_rho, gpu_rho, &desc, width, height, pitch_rho);

  F* gpu_output_rho;
  cudaMalloc((void**)&gpu_output_rho, image_size);

  F* gpu_events_z_u;
  F* gpu_events_z_d;
  F* gpu_events_dl;

  cudaMalloc((void**)&gpu_events_z_u, events_size);
  cudaMalloc((void**)&gpu_events_z_d, events_size);
  cudaMalloc((void**)&gpu_events_dl, events_size);

  cudaMemcpy(
      gpu_events_z_u, cpu_events_z_u, events_size, cudaMemcpyHostToDevice);
  cudaMemcpy(
      gpu_events_z_d, cpu_events_z_d, events_size, cudaMemcpyHostToDevice);
  cudaMemcpy(gpu_events_dl, cpu_events_dl, events_size, cudaMemcpyHostToDevice);

  free(cpu_events_z_u);
  free(cpu_events_z_d);
  free(cpu_events_dl);

  for (int ib = 0; ib < n_iteration_blocks; ++ib) {
    for (int it = 0; it < n_iterations_in_block; ++it) {

      cudaEvent_t start, stop, start_mem_time, stop_mem_time;
      float time;
      float time_all;
      if (verbose) {
        cudaEventCreate(&start);
        cudaEventCreate(&stop);
        cudaEventCreate(&start_mem_time);
        cudaEventCreate(&stop_mem_time);
      } else {
        progress_callback(ib * n_iterations_in_block + it, context);
      }

      cudaMemset(gpu_output_rho, 0, image_size);
      cudaMemcpy2D(gpu_rho,
                   pitch_rho,
                   cpu_rho,
                   sizeof(F) * width,
                   sizeof(F) * width,
                   height,
                   cudaMemcpyHostToDevice);

      if (verbose) {
        cudaEventRecord(start);
        cudaEventRecord(start_mem_time);
      }

#if __CUDACC__
#define reconstruction reconstruction << <blocks, threads>>>
#endif
      reconstruction(detector,
                     gpu_events_z_u,
                     gpu_events_z_d,
                     gpu_events_dl,
                     n_events,
                     gpu_output_rho,
                     n_blocks,
                     n_threads_per_block);

      cudaThreadSynchronize();

      if (verbose) {
        cudaEventRecord(stop);
        cudaEventSynchronize(stop);
        cudaEventElapsedTime(&time, start, stop);
      }

      cudaMemcpy(cpu_rho, gpu_output_rho, image_size, cudaMemcpyDeviceToHost);

      if (verbose) {
        cudaEventRecord(stop_mem_time);
        cudaEventSynchronize(stop_mem_time);
        cudaEventElapsedTime(&time_all, start_mem_time, stop_mem_time);
        printf(
            "[%02d] kernel       : %f ms\n"
            "     kernel + mem : %f ms\n",
            ib * n_iterations_in_block + it,
            time,
            time_all);
      }
    }

    output_callback(detector, ib * n_iterations_in_block, cpu_rho, context);
  }

  if (!verbose) {
    progress_callback(n_iteration_blocks * n_iterations_in_block, context);
  }

#if USE_SENSITIVITY
  cudaUnbindTexture(&tex_sensitivity);
  cudaFree(gpu_sensitivity);
#endif
  cudaUnbindTexture(&tex_rho);
  cudaFree(gpu_rho);
  cudaFree(gpu_events_z_u);
  cudaFree(gpu_events_z_d);
  cudaFree(gpu_events_dl);
  cudaFree(gpu_output_rho);
  free(cpu_rho);
}

template void run_gpu_reconstruction<float>(
    StripDetector<float>& detector,
    Event<float>* events,
    int n_events,
    int n_iteration_blocks,
    int n_iterations_in_block,
    void (*output_callback)(StripDetector<float>& detector,
                            int iteration,
                            float* image,
                            void* context),
    void (*progress_callback)(int iteration, void* context),
    void* context,
    int device,
    int n_blocks,
    int n_threads_per_block,
    bool verbose);
