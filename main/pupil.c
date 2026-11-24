#include <math.h>
#include <stdint.h>

#include "pupil.h"

#define HIGHLIGHT_X 117
#define HIGHLIGHT_Y 116
#define HIGHLIGHT_RADIUS_SQ 6
#define TAU 6.28318530717958647692f

static int pulse_diameter(int32_t elapsed_ms)
{
    const float turns = (float)elapsed_ms / (float)PUPIL_PULSE_MS;
    const float cosine = cosf(turns * TAU);
    int diameter = (int)lroundf(32.0f + 16.0f * cosine);

    if (diameter < PUPIL_DIAMETER_MIN) {
        diameter = PUPIL_DIAMETER_MIN;
    }
    if (diameter > PUPIL_DIAMETER_MAX) {
        diameter = PUPIL_DIAMETER_MAX;
    }
    return diameter;
}

void pupil_pose(int64_t elapsed_ms, pupil_pose_t *pose)
{
    const int64_t pulse_span = (int64_t)PUPIL_PULSE_MS * PUPIL_PULSE_COUNT;
    const int64_t rest_end = pulse_span + PUPIL_REST_MS;
    const int64_t shrink_end = rest_end + PUPIL_BRIDGE_MS;
    const int64_t spin_end = shrink_end + PUPIL_SPIN_MS;
    const int64_t period = spin_end + PUPIL_BRIDGE_MS;

    if (elapsed_ms < 0) {
        elapsed_ms = 0;
    }
    elapsed_ms %= period;

    pose->offset_x = 0.0f;
    pose->offset_y = 0.0f;

    if (elapsed_ms < pulse_span) {
        pose->diameter = pulse_diameter((int32_t)(elapsed_ms % PUPIL_PULSE_MS));
        return;
    }

    if (elapsed_ms < rest_end) {
        pose->diameter = PUPIL_DIAMETER_MAX;
        return;
    }

    if (elapsed_ms < shrink_end) {
        pose->diameter = pulse_diameter((int32_t)(elapsed_ms - rest_end));
        return;
    }

    if (elapsed_ms >= spin_end) {
        pose->diameter = pulse_diameter((int32_t)(PUPIL_BRIDGE_MS + (elapsed_ms - spin_end)));
        return;
    }

    const float phase = (float)(elapsed_ms - shrink_end) / (float)PUPIL_SPIN_MS;
    const float theta = phase * ((float)PUPIL_SPIN_TURNS * TAU);
    float radius;
    if (phase < 0.5f) {
        const float t = phase / 0.5f;
        radius = PUPIL_ORBIT_RADIUS * (t * t * (3.0f - 2.0f * t));
    } else {
        const float t = (phase - 0.5f) / 0.5f;
        radius = PUPIL_ORBIT_RADIUS * (1.0f - (t * t * (3.0f - 2.0f * t)));
    }
    pose->diameter = PUPIL_DIAMETER_MIN;
    pose->offset_x = radius * cosf(theta);
    pose->offset_y = radius * sinf(theta);
}

int pupil_diameter(int32_t elapsed_ms)
{
    pupil_pose_t pose;
    pupil_pose(elapsed_ms, &pose);
    return pose.diameter;
}

static int inside_pupil(int x, int y, int diameter, float offset_x, float offset_y)
{
    const float dx = 2.0f * ((x + 0.5f) - (120.0f + offset_x));
    const float dy = 2.0f * ((y + 0.5f) - (120.0f + offset_y));
    return dx * dx + dy * dy <= (float)diameter * (float)diameter;
}

static int inside_highlight(int x, int y, float offset_x, float offset_y)
{
    const float dx = x - (HIGHLIGHT_X + offset_x);
    const float dy = y - (HIGHLIGHT_Y + offset_y);
    return dx * dx + dy * dy <= HIGHLIGHT_RADIUS_SQ;
}

uint16_t pupil_scene_color(int x, int y, int diameter, float offset_x, float offset_y)
{
    if (diameter < PUPIL_DIAMETER_MIN) {
        diameter = PUPIL_DIAMETER_MIN;
    }
    if (diameter > PUPIL_DIAMETER_MAX) {
        diameter = PUPIL_DIAMETER_MAX;
    }

    if (inside_pupil(x, y, diameter, offset_x, offset_y)) {
        return inside_highlight(x, y, offset_x, offset_y) ? PUPIL_COLOR_SCLERA : PUPIL_COLOR_BLACK;
    }
    return PUPIL_COLOR_SCLERA;
}

void pupil_render_stripe(uint16_t *pixels, int y0, int line_count, int diameter, float offset_x, float offset_y)
{
    for (int row = 0; row < line_count; row++) {
        const int y = y0 + row;
        for (int x = 0; x < PUPIL_VIEW_SIZE; x++) {
            pixels[row * PUPIL_VIEW_SIZE + x] = pupil_scene_color(x, y, diameter, offset_x, offset_y);
        }
    }
}
