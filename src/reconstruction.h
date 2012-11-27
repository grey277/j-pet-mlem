#pragma once

#include <iostream>
#include <vector>
#include <list>

#include "detector_ring.h"

#define LOCATION(x,y,size)  (x*size) +y

template <typename F = double>
class reconstruction {
public:
  typedef std::pair<size_t, size_t> pixel_location;
  typedef std::pair<size_t, size_t> lor_type;
  typedef std::vector<F> output_type;

  struct hits_per_pixel {
    pixel_location location;
    F probability;
  };

  struct mean_per_lor {
    lor_type lor;
    int n;
  };

  typedef uint32_t file_int;
  typedef uint16_t file_half;

  reconstruction(int n_iter, std::string matrix, std::string mean)
  : n_iter(n_iter)
  {
    ibstream in(matrix);

    typedef uint32_t file_int;
    typedef uint16_t file_half;

    file_int in_magic;

    in >> in_magic;

    if (in_magic != detector_ring<F>::magic_f) {
      throw("invalid input system matrix file");
    }

    in >> n_pixels;
    in >> emissions;
    in >> n_tubes;

    scale = new F[n_pixels*n_pixels]();

    std::ifstream mean_file(mean);
    std::vector<mean_per_lor> lor_mean;

    for (;;) {

      int x,y,value;

      if (mean_file.eof()) break;

      mean_file >> y >> x >> value;

      mean_per_lor temp_obj;

      lor_type lor(x,y);

      temp_obj.lor = lor;
      temp_obj.n = value;

      lor_mean.push_back(temp_obj);
    }

    std::vector<hits_per_pixel> pixels;

    int index = 0;

    for (;;) {

      if (in.eof()) break;

      file_half a, b;
      in >> a >> b;
      lor_type lor(a,b);

      n.push_back(get_mean_per_lor(a,b,lor_mean));

#if DEBUG
      if (get_mean_per_lor(a,b,lor_mean) != 0) { printf("get_mean: %d\n", get_mean_per_lor(a,b,lor_mean)); }
#endif

      file_int count;

      in >> count;

      for (int i = 0;i < count; ++i) {

        file_half x, y;
        file_int hits;

        in >> x >> y >> hits;
        pixel_location pixel(x,y);
        hits_per_pixel data;
        data.location = pixel;
        data.probability = static_cast<F>(hits/static_cast<F>(emissions));

        scale[LOCATION(x,y,n_pixels)] += data.probability;

        pixels.push_back(data);

      }

      system_matrix.push_back(pixels);
      pixels.clear();
      index++;
    }

    std::cout
      << "   Pixels: " << n_pixels  << std::endl
      << "Emissions: " << emissions << std::endl
      << "    Tubes: " << n_tubes   << std::endl
      << "     LORs: " << system_matrix.size() << std::endl;
  }

  ~reconstruction() {
    delete [] scale;
  }

  output_type emt() {

    F y[n_pixels * n_pixels];
    std::vector<F> u(system_matrix.size(),0.f);
    std::vector<F> rho(n_pixels * n_pixels,1.0f);

    for (int i = 0; i < n_iter; ++i) {
      std::cout << ".";
      std::cout.flush();

      for (int p = 0; p < n_pixels * n_pixels;++p) {
        y[p] = 0.f;
      }

      int t = 0;
      for (auto it_vector = system_matrix.begin(); it_vector != system_matrix.end(); it_vector++) {
        u[t] = 0.0f;
        for (auto it_list = it_vector->begin(); it_list != it_vector->end(); it_list++) {
          u[t] += rho[LOCATION(it_list->location.first, it_list->location.second, n_pixels)] * it_list->probability;
        }
        t++;
      }

      t = 0;
      for (auto it_vector=system_matrix.begin(); it_vector != system_matrix.end(); ++it_vector) {

        if (n[t]> 0) {
          F phi = n[t]/u[t];// n[t]/u[t];

          for (auto it_list = it_vector->begin(); it_list != it_vector->end(); ++it_list) {
            y[LOCATION(it_list->location.first, it_list->location.second, n_pixels)] += phi * it_list->probability;
          }
        }
        t++;
      }

      for (int p = 0; p < n_pixels * n_pixels; ++p) {
        if (scale[p] > 0) {
          rho[p] *= (y[p]/scale[p]) ;
        }
        // rho[LOCATION(x,p,n_pixels)] = rho[LOCATION(x,p,n_pixels)] * y[LOCATION(x,p,n_pixels)];
      }
    }
    std::cout << std::endl;

    return rho;
  }

  int get_n_pixels() { return n_pixels; }

private:

  int n_tubes;
  int n_pixels;
  int n_iter;
  int emissions;
  int n_lors;
  std::vector< std::vector<hits_per_pixel> > system_matrix;
  std::vector<int> list_of_lors;
  std::vector<int> n;
  F *scale;

  int get_mean_per_lor(file_half &a,file_half &b,std::vector<mean_per_lor> &mean) {
    for (auto it = mean.begin(); it != mean.end(); ++it) {
      if (a == it->lor.first && b == it->lor.second) {
#if DEBUG
        std::cout << "Equal: lor(" << it->lor.first << "," << it->lor.second << ")" << std::endl;
#endif
        return it->n;
      }
    }

    return 0;
  }

};
