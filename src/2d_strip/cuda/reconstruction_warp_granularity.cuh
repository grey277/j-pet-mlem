#pragma once

#include <cuda_runtime.h>

#include "config.h"
#include "event.cuh"
#include "reconstruction_methods.cuh"

template <typename F>
__global__ void reconstruction_2d_strip_cuda(
    CUDA::Config cfg,
    soa_event<F>* soa_data,
    Event<F>* event_list,
    int event_list_size,
    F* output_image,
    F* rho,
    cudaTextureObject_t sensitiviy_tex) {
  short sh_mem_pixel_buffor[20 * 512];

  int tid = (blockIdx.x * blockDim.x) + threadIdx.x;

  F inv_c[3];

  inv_c[0] = cfg.inv_pow_sigma_z;
  inv_c[1] = cfg.inv_pow_sigma_z;
  inv_c[2] = cfg.inv_pow_sigma_dl;

  F half_grid_size = F(0.5) * cfg.grid_size_y;
  F half_pixel_size = F(0.5) * cfg.pixel_size;

  int offset;

  int number_of_blocks =
      int(ceilf(event_list_size /
                (cfg.n_blocks * (cfg.n_threads_per_block / WARP_SIZE))));

  int block_size = (cfg.n_blocks * (cfg.n_threads_per_block / WARP_SIZE));

  int thread_warp_index = floorf(threadIdx.x / WARP_SIZE);

  F sqrt_det_correlation_matrix =
      sqrt(cfg.inv_pow_sigma_z * cfg.inv_pow_sigma_z * cfg.inv_pow_sigma_dl);

  for (int i = 0; i < number_of_blocks; ++i) {

    // calculate offset in event memory location for warp
    int warp_id =
        (i * block_size + (blockIdx.x * (cfg.n_threads_per_block / WARP_SIZE)) +
         thread_warp_index);

    if ((warp_id < event_list_size)) {

      F z_u = soa_data->z_u[warp_id];
      F z_d = soa_data->z_d[warp_id];
      F delta_l = soa_data->dl[warp_id];

      F acc = 0;

      // angle space transformation
      F tn = event_tan(z_u, z_d, cfg.R_distance);
      F y = event_y(delta_l, tn);
      F z = event_z(z_u, z_d, y, tn);

      F angle = atanf(tn);

      F cos = __cosf(angle);

      F sec = 1 / cos;
      F sec_sq = sec * sec;

      F A = (((4 / (cos * cos)) * cfg.inv_pow_sigma_dl) +
             (2 * tn * tn * cfg.inv_pow_sigma_z));
      F B = -4 * tn * cfg.inv_pow_sigma_z;
      F C = 2 * cfg.inv_pow_sigma_z;
      F B_2 = (B / 2) * (B / 2);

      F bb_y = bby(A, C, B_2);
      F bb_z = bbz(A, C, B_2);

#if DEBUG
      if (tid == 0 && i == 0) {
        printf("A: %f B: %f C: %f B_2: %f\n", A, B, C, B_2);
        printf("bb_y: %f bb_z: %f \n", bb_y, bb_z);
      }
#endif

      int2 center_pixel = pixel_location(y,
                                         z,
                                         cfg.pixel_size,
                                         cfg.pixel_size,
                                         cfg.grid_size_y,
                                         cfg.grid_size_z);

      // bounding box limits for event
      int2 ur =
          make_int2(center_pixel.x - pixels_in_line(bb_y, cfg.pixel_size),
                    center_pixel.y + pixels_in_line(bb_z, cfg.pixel_size));
      int2 dl =
          make_int2(center_pixel.x + pixels_in_line(bb_y, cfg.pixel_size),
                    center_pixel.y - pixels_in_line(bb_z, cfg.pixel_size));
      float2 pp;

      int2 ul =
          make_int2(center_pixel.x - pixels_in_line(bb_y, cfg.pixel_size),
                    center_pixel.y - pixels_in_line(bb_z, cfg.pixel_size));

#if DEBUG
      if (tid == 0 && i == 0) {
        printf("UR:= %d Limit:= %d \n", ur.x, ur.y);
        printf("DL:= %d Limit:= %d \n", dl.x, dl.y);
        printf("iz:= %d Limit:= %d \n", dl.y, ur.y);
        printf("iy:= %d Limit:= %d \n", ur.x, dl.x);
      }
#endif

      int bb_size = ceilf(((ur.y - ul.y) * (dl.x - ur.x)) / WARP_SIZE);

      int pixel_count = 0;

      int2 start_warp = ul;
      int2 tid_pixel;

      for (int k = 0; k < bb_size; ++k) {

        warp_space_pixel(tid_pixel, offset, start_warp, ul, ur, dl, tid);

        pp = pixel_center(tid_pixel.x,
                          tid_pixel.y,
                          cfg.pixel_size,
                          cfg.pixel_size,
                          cfg.grid_size_y,
                          cfg.grid_size_z,
                          half_grid_size,
                          half_pixel_size);

        if (in_ellipse(A, B, C, y, z, pp)) {

          pp.x -= y;
          pp.y -= z;

          F event_kernel = main_kernel<F>(y,
                                          tn,
                                          sec,
                                          sec_sq,
                                          pp,
                                          inv_c,
                                          cfg,
                                          sqrt_det_correlation_matrix) /
                           tex2D<F>(sensitiviy_tex, tid_pixel.x, tid_pixel.y);
#if DEBUG
          if (tid < 32 && i == 0) {
                        printf("TID: %d KERNEL: %e PIXEL: %d %d
                        K:%d\n",tid,event_kernel,tid_pixel.x,tid_pixel.y,k);
          }
#endif
          acc += event_kernel *
                 tex2D<F>(sensitiviy_tex, tid_pixel.x, tid_pixel.y) *
                 rho[IMAGE_SPACE_LINEAR_INDEX(tid_pixel.x, tid_pixel.y)];

          sh_mem_pixel_buffor[SH_MEM_INDEX(threadIdx.x, pixel_count, 0)] =
              tid_pixel.x;
          sh_mem_pixel_buffor[SH_MEM_INDEX(threadIdx.x, pixel_count, 1)] =
              tid_pixel.y;
          pixel_count++;
        }
      }

      for (int xor_iter = 16; xor_iter >= 1; xor_iter /= 2) {
        acc += __shfl_xor(acc, xor_iter, WARP_SIZE);
      }

#if DEBUG
      if (tid == 0 && i == 0) {
        printf("KERNEL: %e\n", acc);
      }
#endif

      F inv_acc = 1 / acc;

      for (int k = 0; k < pixel_count; ++k) {

        tid_pixel.x = sh_mem_pixel_buffor[SH_MEM_INDEX(threadIdx.x, k, 0)];
        tid_pixel.y = sh_mem_pixel_buffor[SH_MEM_INDEX(threadIdx.x, k, 1)];

        pp = pixel_center(tid_pixel.x,
                          tid_pixel.y,
                          cfg.pixel_size,
                          cfg.pixel_size,
                          cfg.grid_size_y,
                          cfg.grid_size_z,
                          half_grid_size,
                          half_pixel_size);

        pp.x -= y;
        pp.y -= z;

        F event_kernel = main_kernel<F>(y,
                                        tn,
                                        sec,
                                        sec_sq,
                                        pp,
                                        inv_c,
                                        cfg,
                                        sqrt_det_correlation_matrix) /
                         tex2D<F>(sensitiviy_tex, tid_pixel.x, tid_pixel.y);

#if DEBUG
        if (tid < 32 && i == 0 && k < loop_index) {
                    printf("TID: %d KERNEL: %e PIXEL: %d %d
                    K:%d\n",tid,event_kernel,tid_pixel.x,tid_pixel.y,k);
        }
#endif

        atomicAdd(&output_image[BUFFER_LINEAR_INDEX(tid_pixel.x, tid_pixel.y)],
                  (event_kernel *
                   rho[IMAGE_SPACE_LINEAR_INDEX(tid_pixel.x, tid_pixel.y)]) *
                      inv_acc * sec_sq);
      }
    }
  }
}
