#include <iostream>
#include <cstdio>

#include "util/test.h"
#include "3d/geometry/phantom_builder.h"

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include "3d/geometry/event_generator.h"
#include "3d/geometry/phantom_builder.h"

TEST("rapid_json") {
  FILE* in = fopen("src/3d/hybrid/point_source.js", "r");
  if (!in) {
    std::cerr << "cannot open src/3d/hybrid/point_source.js\n";
  }

  rapidjson::Document doc;
  char readBuffer[256];
  rapidjson::FileReadStream input_stream(in, readBuffer, sizeof(readBuffer));
  doc.ParseStream(input_stream);

  REQUIRE(doc.IsArray());
  rapidjson::Value& obj = doc[0];

  REQUIRE(obj.HasMember("type"));
  REQUIRE(obj.HasMember("angular"));
  fclose(in);
}

TEST("PhantomBuilder/angular_distribution") {
  FILE* in = fopen("src/3d/hybrid/point_source.js", "r");
  if (!in) {
    std::cerr << "cannot open src/3d/hybrid/point_source.js\n";
  }

  rapidjson::Document doc;
  char readBuffer[256];
  rapidjson::FileReadStream input_stream(in, readBuffer, sizeof(readBuffer));
  doc.ParseStream(input_stream);
  fclose(in);

  rapidjson::Value& obj = doc[0];
  rapidjson::Value& angular_val = obj["angular"];

  PET3D::SingleDirectionDistribution<float> distr =
      PET3D::create_from_json<PET3D::SingleDirectionDistribution<float>>(
          angular_val);

  REQUIRE(distr.direction.x == Approx(1.0f / std::sqrt(2.0f)).epsilon(1e-7));
  REQUIRE(distr.direction.y == Approx(0.0f).epsilon(1e-7));
  REQUIRE(distr.direction.z == Approx(1.0f / std::sqrt(2.0f)).epsilon(1e-7));

  int dummy;
  auto dir = distr(dummy);

  REQUIRE(dir.x == Approx(1.0f / std::sqrt(2.0f)).epsilon(1e-7));
  REQUIRE(dir.y == Approx(0.0f).epsilon(1e-7));
  REQUIRE(dir.z == Approx(1.0f / std::sqrt(2.0f)).epsilon(1e-7));
}
