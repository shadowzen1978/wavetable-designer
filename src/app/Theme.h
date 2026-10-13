#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace wtd {
struct Theme {
    juce::Colour background, panel, text, muted, grid, accent;
};
inline Theme themeFor(bool dark) {
    return dark
        ? Theme { juce::Colour(0xff15191f), juce::Colour(0xff1d242d),
                  juce::Colour(0xffedf2f7), juce::Colour(0xffa1adbc),
                  juce::Colour(0xff354150), juce::Colour(0xff7ed4d4) }
        : Theme { juce::Colour(0xfff2f5f8), juce::Colour(0xffffffff),
                  juce::Colour(0xff182334), juce::Colour(0xff526175),
                  juce::Colour(0xffd3dce6), juce::Colour(0xff087878) };
}
} // namespace wtd
