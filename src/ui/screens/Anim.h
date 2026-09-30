#pragma once

#include <algorithm>
#include <cmath>

namespace romantasy::ui::anim
{
    // CSS cubic-bezier(x1, y1, x2, y2): solve the curve's parameter for a
    // given x (time), then evaluate y (progress). Newton iterations with a
    // clamped parameter are plenty for UI easing.
    struct CubicBezier
    {
        float x1;
        float y1;
        float x2;
        float y2;

        [[nodiscard]] static float Sample(float a1, float a2, float t) noexcept
        {
            return ((1.0f - 3.0f * a2 + 3.0f * a1) * t + (3.0f * a2 - 6.0f * a1)) * t * t + 3.0f * a1 * t;
        }

        [[nodiscard]] static float Slope(float a1, float a2, float t) noexcept
        {
            return 3.0f * (1.0f - 3.0f * a2 + 3.0f * a1) * t * t + 2.0f * (3.0f * a2 - 6.0f * a1) * t + 3.0f * a1;
        }

        [[nodiscard]] float operator()(float x) const noexcept
        {
            if (x <= 0.0f) {
                return 0.0f;
            }
            if (x >= 1.0f) {
                return 1.0f;
            }
            float t = x;
            for (int i = 0; i < 8; ++i) {
                const float slope = Slope(x1, x2, t);
                if (std::fabs(slope) < 1e-6f) {
                    break;
                }
                const float error = Sample(x1, x2, t) - x;
                t = std::clamp(t - error / slope, 0.0f, 1.0f);
            }
            return std::clamp(Sample(y1, y2, t), 0.0f, 1.0f);
        }
    };

    inline constexpr CubicBezier EaseOut{ 0.16f, 1.0f, 0.30f, 1.0f };   // --ease-out
    inline constexpr CubicBezier EaseSoft{ 0.22f, 0.61f, 0.36f, 1.0f }; // --ease-soft

    struct Tween
    {
        float duration = 0.0f;
        float elapsed = 0.0f;
        bool running = false;

        void Start(float durationSeconds) noexcept
        {
            duration = std::max(durationSeconds, 0.0f);
            elapsed = 0.0f;
            running = duration > 0.0f;
        }

        void Finish() noexcept
        {
            elapsed = duration;
            running = false;
        }

        float Advance(float dt) noexcept
        {
            if (running) {
                elapsed = std::min(elapsed + std::max(dt, 0.0f), duration);
                if (elapsed >= duration) {
                    running = false;
                }
            }
            return Progress();
        }

        [[nodiscard]] float Progress() const noexcept
        {
            if (duration <= 0.0f) {
                return 1.0f;
            }
            return std::clamp(elapsed / duration, 0.0f, 1.0f);
        }

        [[nodiscard]] bool Running() const noexcept { return running; }
        [[nodiscard]] bool Done() const noexcept { return !running && elapsed >= duration; }
    };

    [[nodiscard]] inline float Pulse(float seconds, float periodSeconds) noexcept
    {
        if (periodSeconds <= 0.0f) {
            return 0.0f;
        }
        return 0.5f - 0.5f * std::cos(6.28318530f * seconds / periodSeconds);
    }
}
