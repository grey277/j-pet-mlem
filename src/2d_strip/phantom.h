
#pragma once

#include <cmath>
#include <vector>
#include <fstream>
#include <ctime>
#include <random>
#include <algorithm>

#if _OPENMP
#include <omp.h>
#else
#define omp_get_max_threads() 1
#define omp_get_thread_num() 0
#endif

#define MAIN_PHANTOM 1

#include "event.h"
#include "util/bstream.h"

typedef std::minstd_rand0 rng;

template <typename T> int sgn(T val) { return (T(0) < val) - (val < T(0)); }

template <typename T = double> class Phantom {

  typedef std::pair<int, int> Pixel;
  typedef std::pair<T, T> Point;

 private:
  int iteration;
  int n_pixels;
  T pixel_size;
  T R_distance;
  T Scentilator_length;
  T sin;
  T cos;
  T inv_a2;
  T inv_b2;
  T sigma_z;
  T sigma_dl;
  std::vector<EllipseParameters<T>> ellipse_list;
  std::vector<Event<T>> event_list;
  std::vector<std::vector<T>> output;
  std::vector<std::vector<T>> output_without_errors;
  static constexpr const T PI_2 = T(1.5707963);
  static constexpr const T radian = T(M_PI / 180);

 public:
  Phantom(std::vector<EllipseParameters<T>>& el,
          int n_pixels,
          T& pixel_size,
          T& R_distance,
          T& Scentilator_length,
          T sigma_z,
          T sigma_dl)
      : n_pixels(n_pixels),
        pixel_size(pixel_size),
        R_distance(R_distance),
        Scentilator_length(Scentilator_length),
        sigma_z(sigma_z),
        sigma_dl(sigma_dl) {
    ellipse_list = el;
    output.assign(n_pixels, std::vector<T>(n_pixels, T(0)));
    output_without_errors.assign(n_pixels, std::vector<T>(n_pixels, T(0)));
  }

  bool in(T y, T z, EllipseParameters<T> el) const {

    T dy = (y - el.y);
    T dz = (z - el.x);
    T d1 = (sin * dy + cos * dz);  // y
    T d2 = (sin * dz - cos * dy);  // z

    return ((d1 * d1 / (el.a * el.a)) + (d2 * d2 / (el.b * el.b))) <= T(1)
               ? true
               : false;
  }

  void emit_event() {

    std::vector<std::vector<Event<T>>> event_list_per_thread;

    event_list_per_thread.resize(omp_get_max_threads());

    for (auto& el : ellipse_list) {

      T max = std::max(el.a, el.b);

      std::cout << el.x << " " << el.y << " " << el.a << " " << el.b << " "
                << el.angle << std::endl;

      std::uniform_real_distribution<T> uniform_dist(0, 1);
      std::uniform_real_distribution<T> uniform_angle(-1, 1);

      std::uniform_real_distribution<T> uniform_y(el.y - max, el.y + max);
      std::uniform_real_distribution<T> uniform_z(el.x - max, el.x + max);
      std::normal_distribution<T> normal_dist_dz(0, sigma_z);
      std::normal_distribution<T> normal_dist_dl(0, sigma_dl);
      rng rd;

      std::vector<rng> rng_list;

      for (int i = 0; i < omp_get_max_threads(); ++i) {

        rng_list.push_back(rd);
        rng_list[i].seed(42 + (3453 * i));
        // OR
        // Turn on leapfrogging with an offset that depends on the task id
      }

      sin = std::sin(T(el.angle * radian));
      cos = std::cos(T(el.angle * radian));
      inv_a2 = T(1) / (el.a * el.a);
      inv_b2 = T(1) / (el.b * el.b);
      iteration = el.iter;

#if _OPENMP
#pragma omp for schedule(static)
#endif
      for (int i = 0; i < iteration; ++i) {

#if MAIN_PHANTOM

        T ry, rz, rangle;

        ry = uniform_y(rng_list[omp_get_thread_num()]);
        rz = uniform_z(rng_list[omp_get_thread_num()]);
        rangle = M_PI_4 * uniform_angle(rng_list[omp_get_thread_num()]);

        if (in(ry, rz, el) && (std::abs(rangle) != M_PI_2)) {
          T z_u, z_d, dl;

          z_u = rz + (R_distance - ry) * std::tan(rangle);
          z_d = rz - (R_distance + ry) * std::tan(rangle);
          dl = -2 * ry * std::sqrt(1 + (std::tan(rangle) * std::tan(rangle)));

          z_u += normal_dist_dz(rng_list[omp_get_thread_num()]);
          z_d += normal_dist_dz(rng_list[omp_get_thread_num()]);
          dl += normal_dist_dl(rng_list[omp_get_thread_num()]);

          if (std::abs(z_u) < (Scentilator_length / T(2)) &&
              std::abs(z_d) < (Scentilator_length / T(2))) {

            Event<T> event(z_u, z_d, dl);

            T tan = event.tan(R_distance);
            T y = event.y(tan);
            T z = event.z(y, tan);

            Pixel p = pixel_location(y, z);
            Pixel pp = pixel_location(ry, rz);

            output[p.first][p.second]++;
            output_without_errors[pp.first][pp.second]++;

            event_list_per_thread[omp_get_thread_num()].push_back(event);
          }
        }

#else

        ry = el.y + normal_dist_dl(rng_list[omp_get_thread_num()]);
        rz = el.x + normal_dist_dz(rng_list[omp_get_thread_num()]);

        T y = ry;
        T z = rz;

        Pixel p = pixel_location(x, y);
        Pixel pp = pixel_location(0, 0);

        output[p.first][p.second]++;
        output_without_errors[pp.first][pp.second]++;

        temp_event.z_u = ry;
        temp_event.z_d = rz;
        temp_event.dl = 0;
        start++;

        event_list_per_thread[omp_get_thread_num()].push_back(temp_event);

#endif
      }

      for (signed i = 0; i < omp_get_max_threads(); ++i) {

        event_list.insert(event_list.end(),
                          event_list_per_thread[i].begin(),
                          event_list_per_thread[i].end());
      }

      std::cout << "VECTOR: " << event_list.size() << std::endl;
    }

    // here

    // output reconstruction PNG

    std::ostringstream strs;
    strs << ellipse_list[0].angle;
    std::string str = strs.str();

    std::string file = std::string("phantom.png");

    png_writer png(file);
    png.write_header<>(n_pixels, n_pixels);

    T output_max = 0;
    for (auto& col : output) {
      for (auto& row : col)
        output_max = std::max(output_max, row);
    }

    auto output_gain =
        static_cast<double>(std::numeric_limits<uint8_t>::max()) / output_max;

    for (int y = 0; y < n_pixels; ++y) {
      uint8_t row[n_pixels];
      for (auto x = 0; x < n_pixels; ++x) {
        row[x] =
            std::numeric_limits<uint8_t>::max() - output_gain * output[y][x];
      }
      png.write_row(row);
    }

    png_writer png_true("phantom_true.png");
    png_true.write_header<>(n_pixels, n_pixels);

    output_max = 0;
    for (auto& col : output_without_errors) {
      for (auto& row : col)
        output_max = std::max(output_max, row);
    }

    output_gain =
        static_cast<double>(std::numeric_limits<uint8_t>::max()) / output_max;

    for (int y = 0; y < n_pixels; ++y) {
      uint8_t row[n_pixels];
      for (auto x = 0; x < n_pixels; ++x) {
        row[x] = std::numeric_limits<uint8_t>::max() -
                 output_gain * output_without_errors[y][x];
      }
      png_true.write_row(row);
    }
  }

  // coord Plane
  Pixel pixel_location(T y, T z) {
    return Pixel(std::floor((R_distance - y) / pixel_size),
                 std::floor((R_distance + z) / pixel_size));
  }

  template <typename StreamType> Phantom& operator>>(StreamType& out) {

    typename std::vector<Event<T>>::iterator it;
    int size = event_list.size();
    int i = 0;
    out << size;
    for (it = event_list.begin(); it != event_list.end(); ++it) {
      ++i;
      out << it->z_u << it->z_d << it->dl;
    }
    std::cout << "i: " << i << std::endl;
    return *this;
  }

  template <typename StreamType>
  friend StreamType& operator<<(StreamType& out, Phantom& p) {
    p >> out;
    return out;
  }
};
