#pragma once

#include <cuda_runtime.h>

#define SOA_SIZE 180000000

template <typename F> struct soa_event {

  F z_u[SOA_SIZE];
  F z_d[SOA_SIZE];
  F dl[SOA_SIZE];

  void set_data_chunk(Event<float>* data_chunk, int offset, int aos_data_size) {

    for (int i = 0; i < aos_data_size; i += offset) {

      for (int j = 0; j < offset; ++j) {

        if ((i + j) < aos_data_size) {

          z_u[i + j] = data_chunk[j].z_u;
          z_d[i + j] = data_chunk[j].z_d;
          dl[i + j] = data_chunk[j].dl;
        }
      }
    }
  }

  void set_data(Event<float>* aos_data, int aos_data_size) {
#if DISABLE
    data_size = aos_data_size;
#endif
    for (int i = 0; i < aos_data_size; ++i) {

      z_u[i] = aos_data[i].z_u;
      z_d[i] = aos_data[i].z_d;
      dl[i] = aos_data[i].dl;
    }
  }
#if DISABLE
  T* z_u;
  T* z_d;
  T* dl;
  int malloc_size;
  int data_size;

  soa_event() {

    printf("Allocate the SOA representation for event_list\n");
    z_u = (T*)malloc(N * sizeof(float));
    z_d = (T*)malloc(N * sizeof(float));
    dl = (T*)malloc(N * sizeof(float));
    malloc_size = N;
  }

  void set_data(event<float>* aos_data, int aos_data_size) {

    data_size = aos_data_size;

    for (int i = 0; i < aos_data_size; ++i) {

      z_u[i] = aos_data[i].z_u;
      z_d[i] = aos_data[i].z_d;
      dl[i] = aos_data[i].dl;
    }
  }

  ~soa_event() {
    free(z_u);
    free(z_d);
    free(dl);
    printf("Free memory of SOA representation for event_list\n");
  }
#endif
};
