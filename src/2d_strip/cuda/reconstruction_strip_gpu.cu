#include <cuda_runtime.h>
#include <stdio.h>

#include "reconstruction.cuh"

static cudaError err;

#define cuda_kernel_config(blocks, threads)                      \
  {                                                              \
    printf("Cuda kernel config\n");                              \
    printf("Number of  blocks per kernel: %d\n", blocks);        \
    printf("Number of threads|block per kernel: %d\n", threads); \
  }

#define cuda(f, ...)                                        \
  if ((err = cuda##f(__VA_ARGS__)) != cudaSuccess) {        \
    fprintf(stderr, #f "() %s\n", cudaGetErrorString(err)); \
    exit(-1);                                               \
  }
#define cudathread_per_blockoSync(...) cuda(__VA_ARGS__)

void gpu_reconstruction_strip_2d(gpu_config::GPU_parameters cfg,
                                 event<float>* event_list,
                                 int event_size,
                                 int iteration_chunk) {

  cudaSetDevice(0);

  printf("Data Size: %d \n", event_size);

  dim3 blocks(cfg.number_of_blocks);
  dim3 threads(cfg.number_of_threads_per_block);

  cuda_kernel_config(cfg.number_of_blocks, cfg.number_of_threads_per_block);

  float* cpu_image_buffor =
      (float*)malloc(cfg.n_pixels * cfg.n_pixels * cfg.number_of_blocks);

  float* cpu_image_rho = (float*)malloc(cfg.n_pixels * cfg.n_pixels);

  float* gpu_image_buffor;
  float* gpu_image_rho;

  event<float>* gpu_event_list;

  cuda(Malloc, (void**)&gpu_event_list, event_size * sizeof(event<float>));

  cuda(Malloc,
       (void**)&gpu_image_buffor,
       cfg.n_pixels * cfg.n_pixels * cfg.number_of_blocks * sizeof(float));

  cuda(Malloc,
       (void**)&gpu_image_rho,
       cfg.n_pixels * cfg.n_pixels * sizeof(float));

  cuda(Memcpy,
       gpu_event_list,
       event_list,
       event_size * sizeof(event<float>),
       cudaMemcpyHostToDevice);

  cuda(Memcpy,
       gpu_image_buffor,
       cpu_image_buffor,
       cfg.n_pixels * cfg.n_pixels * cfg.number_of_blocks * sizeof(float),
       cudaMemcpyHostToDevice);

  cuda(Memcpy,
       gpu_image_rho,
       cpu_image_rho,
       cfg.n_pixels * cfg.n_pixels * sizeof(float),
       cudaMemcpyHostToDevice);

  cuda(
      Memset, gpu_image_rho, 1.0f, cfg.n_pixels * cfg.n_pixels * sizeof(float));

  cudaEvent_t start, stop;
  cudaEventCreate(&start);
  cudaEventCreate(&stop);

  cudaEventRecord(start);

  reconstruction_2d_strip_cuda<float> << <blocks, threads>>>
      (cfg, gpu_event_list, event_size, gpu_image_buffor, gpu_image_rho);

  cudaThreadSynchronize();

  cudaEventRecord(stop);

  cudaEventSynchronize(stop);
  float milliseconds = 0;
  cudaEventElapsedTime(&milliseconds, start, stop);

  printf("Direct kernel time without memcpy %f ms\n", milliseconds);

  cuda(Free, gpu_image_buffor);
  cuda(Free, gpu_image_rho);
  free(cpu_image_buffor);
  free(cpu_image_rho);
}
