#include<iostream>
#include<fstream>

#include <catch.hpp>

#include "circle.h"

TEST_CASE("circle/init", "circle initialization") {
  circle<> c1(1.);

  CHECK( c1.radious()  == 1. );
  CHECK( c1.radious2() == 1. );

  circle<> c2(std::sqrt(2.));

  CHECK( c2.radious()  == std::sqrt(2.) ); // exact!
  CHECK( c2.radious2() == Approx(2.)    );
}

TEST_CASE("circle/secant", "circle secant") {
  circle<> c(1);

  SECTION("angle-0", "0 degrees from (0, 0)") {
    decltype(c)::event_type zero(0., 0., 0.);
    auto s = c.secant(zero);

    CHECK( std::min(s.first.x, s.second.x) == Approx(-1.) );
    CHECK( std::max(s.first.x, s.second.x) == Approx( 1.) );

    CHECK( s.first.y  ==  0. );
    CHECK( s.second.y ==  0. );

    auto a = c.secant_angles(zero);
    if (a.first  == Approx(-M_PI)) a.first  += 2. * M_PI;
    if (a.second == Approx(-M_PI)) a.second += 2. * M_PI;

    CHECK( std::min(a.first, a.second) == Approx(0.) );
    CHECK( std::max(a.first, a.second) == Approx(M_PI) );
  }
  SECTION("angle-90", "90 degrees from (0, 0)") {
    decltype(c)::event_type zero90(0., 0., M_PI_2);
    auto s = c.secant(zero90);

    CHECK( s.first.x  == Approx(0.) );
    CHECK( s.second.x == Approx(0.) );

    CHECK( std::min(s.first.y, s.second.y) == -1. );
    CHECK( std::max(s.first.y, s.second.y) ==  1. );

    auto a = c.secant_angles(zero90);
    if (a.first  == Approx(-M_PI)) a.first  += M_2_PI;
    if (a.second == Approx(-M_PI)) a.second += M_2_PI;

    CHECK( std::min(a.first, a.second) == Approx(-M_PI_2) );
    CHECK( std::max(a.first, a.second) == Approx( M_PI_2) );
  }
#if 0
  SECTION("angle-45", "45 degrees from (1, 0)") {
    decltype(c)::event_type xone45(1., 0., M_PI_4);
    auto s = c.secant(xone45);

    CHECK( std::min(s.first.x, s.second.x) == Approx( 0.) );
    CHECK( std::max(s.first.x, s.second.x) == xone45.x    );

    CHECK( std::min(s.first.y, s.second.y) == Approx(-1.) );
    CHECK( std::max(s.first.y, s.second.y) == xone45.y    );
  }
#endif
}

template<typename F>
bool compare(F left , F right, F tol) {
  return(std::abs(left-right)<tol);
}
const double tol=1e-14;

TEST_CASE("circle/secant/math","test  using file generated by mathematica") {
  std::ifstream in("secant.test");

  if(!in) {
    WARN("cannot open file `secant.test'");
    return;
  }

  double r;
  int n_detectors;
  in>>r;
  in>>n_detectors;

  int line=0;
  while(true) {
    double x,y,angle;
    double x1,y1;
    double x2,y2;

    in>>x>>y>>angle;
    in>>x1>>y1>>x2>>y2;
    double angle1, angle2;
    in>>angle1>>angle2;
    int section1, section2;
    in>>section1>>section2;

    if(in.eof())
      break;

    line++;
    circle<double> c(r);
    decltype(c)::event_type event(x, y, angle );

    auto  secant= c.secant(event);

    bool first_to_first= (
                          compare(secant.first.x,x1,tol) &&
                          compare(secant.first.y,y1,tol) &&
                          compare(secant.second.x,x2,tol) &&
                          compare(secant.second.y,y2,tol)
                          ) ;

    bool first_to_second=  (
                             compare(secant.first.x,x2,tol) &&
                             compare(secant.first.y,y2,tol) &&
                             compare(secant.second.x,x1,tol) &&
                             compare(secant.second.y,y1,tol)
                            );
    if( !(first_to_first || first_to_second) )  {
      char msg[128];
      sprintf(msg,"error in intersections line %d\n",line);
      FAIL(msg);
    }
    else {
      auto angles   = c.secant_angles(event);
      auto sections = c.secant_sections(event,n_detectors);

      if(first_to_first)  {

        CHECK(angles.first==Approx(angle1));
        CHECK(angles.second==Approx(angle2));

        CHECK(sections.first==section1);
        CHECK(sections.second==section2);

      } else {

        CHECK(angles.first==Approx(angle2));
        CHECK(angles.second==Approx(angle1));

        CHECK(sections.first==section2);
        CHECK(sections.second==section1);

      }

    }
  }
}
