#pragma once

#include <span>
#include <math.h>
#include <core/types.h>

/// Approx center using iterative least square fit
template <size_t ITERATIONS = 10>
MachinePosXY approximate_center(std::span<const MachinePosXY> points) {
    // Smallest distance / error to deal with
    // Should be safe to div by without precision loss
    static constexpr float NOTHING_MM { 0.000001f };

    // Guess center as centroid
    MachinePosXY center = { { { .x = 0, .y = 0 } } };
    for (const MachinePosXY &p : points) {
        center += p;
    }
    center.x /= points.size();
    center.y /= points.size();

    // Iteratively optimize least squares
    for (unsigned int i = 0; i < ITERATIONS; ++i) {
        // Update radius
        float radius = 0;
        for (const MachinePosXY &p : points) {
            radius += (p - center).magnitude() / points.size();
        }

        // Compute correction
        MachinePosXY correction = { { { .x = 0, .y = 0 } } };
        float residual_square_sum = 0; // Sum of residual squares = weights
        for (const MachinePosXY &point : points) {
            const float d = (point - center).magnitude(); // Point distance to center
            const float R = d - radius; // Point error
            const float Rs = R * R; // Point error square

            // Prevent div by possibly close to zero value
            if (d < NOTHING_MM) {
                continue;
            }

            // Coordinate correction = (coordinate distance / distance) * error * weight
            correction += ((point - center) / d) * R * Rs;
            residual_square_sum += Rs;
        }

        // Prevent div by possibly close to zero value
        if (residual_square_sum < NOTHING_MM) {
            continue;
        }

        // Apply correction
        center += correction / residual_square_sum;
    }

    return center;
}
