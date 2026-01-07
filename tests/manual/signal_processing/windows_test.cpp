#include <signal_processing/windowing.hpp>

#include <iostream>
#include <vector>
#include <span>

int main() {
    constexpr size_t WINDOW_SIZE = 1000;

    // Input is all ones so applying a window yields the window coefficients
    std::vector<float> input(WINDOW_SIZE, 1.0f);

    sp::NoWindow<float> no_win(WINDOW_SIZE);
    sp::OnTheFlyHannWindow<float> onfly(WINDOW_SIZE);
    sp::HannWindow<float> hann(WINDOW_SIZE);

    auto no = input;
    auto on = input;
    auto hn = input;

    no_win.apply(std::span<float> { no.data(), no.size() });
    onfly.apply(std::span<float> { on.data(), on.size() });
    hann.apply(std::span<float> { hn.data(), hn.size() });

    // CSV header and data
    std::cout << "Index,Input,NoWindow,OnTheFlyHann,Hann\n";
    for (size_t i = 0; i < WINDOW_SIZE; ++i) {
        std::cout << i << "," << input[i] << "," << no[i] << "," << on[i] << "," << hn[i] << "\n";
    }

    return 0;
}
