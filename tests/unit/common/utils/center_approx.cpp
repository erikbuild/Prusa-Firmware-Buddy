#include <array>
#include <span>
#include <catch2/catch.hpp>
#include <types.h>

#include <center_approx.hpp>

inline constexpr float ALLOWED_ERROR_MM { 0.01f };

static void test(const std::span<const xy_pos_t> points, const xy_pos_t true_center) {
    const xy_pos_t center = approximate_center(points);
    INFO("Center " << center.x << " " << center.y);
    CHECK((center - true_center).magnitude() < ALLOWED_ERROR_MM);
}

TEST_CASE("Fix circle 3 point") {
    test({ {
             { 184.938f, 180 },
             { 178.163f, 183.181f },
             { 178.163f, 176.819f },
         } },
        { { 180.80372797047968f, 180.0 } }); // from circle_fit import taubinSVD
}

TEST_CASE("Fit circle 12 point") {
    test({ {
             { 185.419f, 180.2f },
             { 184.828f, 182.428f },
             { 183.166f, 183.991f },
             { 180.975f, 184.438f },
             { 178.928f, 183.747f },
             { 177.544f, 182.181f },
             { 177.075f, 180.2f },
             { 177.613f, 178.262f },
             { 179.006f, 176.781f },
             { 180.975f, 176.125f },
             { 183.087f, 176.538f },
             { 184.756f, 178.019f },
         } },
        { { 181.25330479103351f, 180.28446146100904f } }); // from circle_fit import taubinSVD
}
