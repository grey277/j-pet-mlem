#ifndef RECONSTRUCTOR
#define RECONSTRUCTOR

#include <vector>
#include <algorithm>

#include "2d/barrel/lor_info.h"
#include "2d/strip/event.h"

template <typename Scanner, typename Kernel2D> class Reconstructor {
 public:
  using F = typename Scanner::F;
  using S = typename Scanner::S;
  using LorPixelInfo = PET2D::Barrel::LorPixelnfo<F, S>;
  using Response = typename Scanner::Response;
  using LOR = PET2D::Barrel::LOR<S>;
  using StripEvent = PET2D::Strip::Event<F>;
  using PixelInfo = typename LorPixelInfo::PixelInfo;
  using Pixel = typename LorPixelInfo::Pixel;

  struct FrameEvent {
    LOR lor;
    F up;
    F right;
    F tan;
    F half_box_up;
    F half_box_right;
    typename LorPixelInfo::PixelInfoContainer::const_iterator first_pixel;
    typename LorPixelInfo::PixelInfoContainer::const_iterator last_pixel;
    S first_plane;
    S last_plane;
  };

  Reconstructor(const Scanner& scanner,
                const LorPixelInfo& lor_pixel_info,
                F fov_length,
                F z_left)
      : scanner_(scanner),
        lor_pixel_info_(lor_pixel_info),
        fov_length(fov_length),
        z_left(z_left),
        n_planes((int)std::ceil(fov_length / lor_pixel_info_.grid.pixel_size)),
        kernel_(scanner.sigma_z(), scanner.sigma_dl()),
        rho_(lor_pixel_info_.grid.n_columns * lor_pixel_info_.grid.n_rows *
                 n_planes,
             F(0.0)),
        rho_prev_(lor_pixel_info_.grid.n_columns * lor_pixel_info_.grid.n_rows *
                      n_planes,
                  F(1.0)) {}

  FrameEvent translate_to_frame(const Response& response) {
    FrameEvent event;
    event.lor = response.lor;

    auto R = lor_pixel_info_[event.lor].segment.length;
    StripEvent strip_event(response.z_up, response.z_dn, response.dl);

    strip_event.transform(R, event.tan, event.up, event.right);
    F sec, A, B, C;
    kernel_.ellipse_bb(
        event.tan, sec, A, B, C, event.half_box_up, event.half_box_right);

    auto ev_z_left = event.right - event.half_box_right;
    auto ev_z_right = event.right + event.half_box_right;
    event.first_plane = plane(ev_z_left);
    event.last_plane = plane(ev_z_right) + 1;

    auto y_up = event.up + event.half_box_up;
    auto y_dn = event.up - event.half_box_up;
    auto t_up = (y_up + R) / (2 * R);
    auto t_dn = (y_dn + R) / (2 * R);

    PixelInfo pix_info_up, pix_info_dn;
    pix_info_up.t = t_up;
    pix_info_dn.t = t_dn;
    event.last_pixel =
        std::upper_bound(lor_pixel_info_[event.lor].pixels.begin(),
                         lor_pixel_info_[event.lor].pixels.end(),
                         pix_info_up,
                         [](const PixelInfo& a, const PixelInfo& b)
                             -> bool { return a.t < b.t; });

    event.first_pixel =
        std::lower_bound(lor_pixel_info_[event.lor].pixels.begin(),
                         lor_pixel_info_[event.lor].pixels.end(),
                         pix_info_dn,
                         [](const PixelInfo& a, const PixelInfo& b)
                             -> bool { return a.t < b.t; });

    return event;
  }

  S plane(F z) {
    return S(std::floor((z - z_left) / lor_pixel_info_.grid.pixel_size));
  }

  Response fscanf_responses(std::istream& in) {
    auto response = fscanf_response(in);
    while (in) {
      events_.push_back(translate_to_frame(response));
      response = fscanf_response(in);
    }
  }

  int n_events() const { return events_.size(); }
  FrameEvent frame_event(int i) const { return events_[i]; }

  int iterate() {
    auto grid = lor_pixel_info_.grid;
    int i;
    for (i = 0; i < n_events(); ++i) {
      auto event = frame_event(i);

      for (auto it = event.first_pixel; it != event.last_pixel; ++it) {
        auto pix = it->pixel;
        auto ix = pix.x;
        auto iy = pix.y;
        for (int plane = event.first_plane; plane < event.last_plane; ++plane) {
          int index =
              ix + iy * grid.n_columns + plane * grid.n_columns * grid.n_rows;
        }
      }
    }

    return i;
  }

 private:
  Response fscanf_response(std::istream& in) {
    S d1, d2;
    F z_up, z_dn, dl;
    Response response;
    in >> d1 >> d2 >> response.z_up >> response.z_dn >> response.dl;
    response.lor = LOR(d1, d2);
    return response;
  }

  const Scanner& scanner_;
  const LorPixelInfo& lor_pixel_info_;
  const F fov_length;
  const F z_left;
  const S n_planes;
  std::vector<FrameEvent> events_;
  Kernel2D kernel_;
  std::vector<F> rho_;
  std::vector<F> rho_prev_;
};

#endif  // RECONSTRUCTOR
