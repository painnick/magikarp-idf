#pragma once

#include <stdint.h>

#define PUPIL_VIEW_SIZE 240
#define PUPIL_DIAMETER_MIN 16
#define PUPIL_DIAMETER_MAX 48
#define PUPIL_PULSE_MS 5000
#define PUPIL_PULSE_COUNT 2
#define PUPIL_REST_MS 2000
#define PUPIL_SPIN_MS 5400
#define PUPIL_SPIN_TURNS 3
#define PUPIL_BRIDGE_MS (PUPIL_PULSE_MS / 2)
#define PUPIL_ORBIT_RADIUS 40
#define PUPIL_CYCLE_MS PUPIL_PULSE_MS

/* Warm white RGB(255, 252, 246). The whole panel is sclera. */
#define PUPIL_COLOR_SCLERA 0xFFDE
#define PUPIL_COLOR_BLACK 0x0000

typedef struct {
    int diameter;
    float offset_x;
    float offset_y;
} pupil_pose_t;

/* Pose over two shrink-grow pulses, one centered roll, then repeat. */
void pupil_pose(int64_t elapsed_ms, pupil_pose_t *pose);

/* Diameter of the pose at elapsed_ms. */
int pupil_diameter(int32_t elapsed_ms);

/* Color of one screen pixel. Offsets move the pupil from screen center, in pixels. */
uint16_t pupil_scene_color(int x, int y, int diameter, float offset_x, float offset_y);

/* Raster `line_count` rows starting at `y0` into a PUPIL_VIEW_SIZE-wide buffer. */
void pupil_render_stripe(uint16_t *pixels, int y0, int line_count, int diameter, float offset_x, float offset_y);
