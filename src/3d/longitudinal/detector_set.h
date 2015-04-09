#include <iostream>

#include "2d/barrel/detector_set.h"
#include "3d/geometry/event.h"

/// Three-dimensional PET
namespace PET3D {
/// Three-dimensional PET with longitudinal direction(z) added to the barrel
/// detector
namespace Longitudinal {

/// 3D detector made of several scintillators

template <typename DetectorSet2D,
          std::size_t MaxDetectors,
          typename FType = typename DetectorSet2D::F,
          typename SType = typename DetectorSet2D::S>
class DetectorSet {
 public:
  using Point = PET3D::Point<FType>;
  using Point2D = PET2D::Point<FType>;
  using Event = PET3D::Event<FType>;
  using LOR = typename DetectorSet2D::LOR;
  using Indices = typename DetectorSet2D::Indices;
  using BarrelEvent = typename DetectorSet2D::Event;

  struct Response {
    LOR lor;
    FType z_up;
    FType z_dn;
    FType dl;
  };


  DetectorSet(const DetectorSet2D& barrel_a, FType length_a)
      : barrel(barrel_a), length(length_a), half_length(length / 2) {}

  bool escapes_through_endcap(const Event& event) {
    if (event.direction.z == 0.0)
      return false;

    FType r2 = barrel.radius() * barrel.radius();

    {
      FType t_right = (half_length - event.origin.z) / event.direction.z;
      FType x_right = event.origin.x + t_right * event.direction.x;
      FType y_right = event.origin.y + t_right * event.direction.y;
      if (x_right * x_right + y_right * y_right < r2)
        return true;
    }
    {
      FType t_left = -(half_length + event.origin.z) / event.direction.z;
      FType x_left = event.origin.x + t_left * event.direction.x;
      FType y_left = event.origin.y + t_left * event.direction.y;
      if (x_left * x_left + y_left * y_left < r2)
        return true;
    }

    return false;
  }

  template <class RandomGenerator, class AcceptanceModel>
  _ short detect(RandomGenerator& gen,    ///< random number generator
                 AcceptanceModel& model,  ///< acceptance model
                 const Event& e,          ///< event to be detected
                 LOR& lor,                ///<[out] lor of the event
                 FType& position          ///<[out] position of the event
                 ) const {

    Indices left, right;
    BarrelEvent event_xy = e.to_barrel_event();

    barrel.close_indices(event_xy, left, right);

    return 0;
  }

  template <class RandomGenerator, class AcceptanceModel>
  _ bool check_for_hits(RandomGenerator& gen,
                        AcceptanceModel& model,
                        const Indices& indices,
                        Event e,
                        BarrelEvent e_xy,
                        SType& detector,
                        FType& depth,
                        Point& p1,
                        Point& p2) const {

    for (auto i : indices) {
      Point2D p1_xy, p2_xy;
      if (barrel.did_intersect(e_xy, i, p1_xy, p2_xy)) {
        if (did_deposit(gen, model, p1, p2, depth)) {
          detector = i;
          return true;
        }
      }
    }
    return false;
  }

 private:
  const DetectorSet2D& barrel;
  const FType length;
  const FType half_length;
};

}  // Longitudinal
}  // PET3D
