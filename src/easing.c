#include "ezt.h"

float ez_clamp01(float v)
{
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

float ez_lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float ez_approach(float cur, float target, float dt, float speed)
{
    if (dt <= 0.0f || speed <= 0.0f) return cur;
    float t = 1.0f - __builtin_expf(-dt * speed);
    return cur + (target - cur) * t;
}

float ez_ease_out_cubic(float t)
{
    t = ez_clamp01(t);
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float ez_ease_in_out(float t)
{
    t = ez_clamp01(t);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - __builtin_powf(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

float ez_ease_out_back(float t)
{
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    t = ez_clamp01(t) - 1.0f;
    return 1.0f + c3 * t * t * t + c1 * t * t;
}
