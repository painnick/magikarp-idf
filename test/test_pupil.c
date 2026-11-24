#include <math.h>
#include <stdio.h>

#include "pupil.h"

static int g_failed;

static void expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "FAIL %s: got %d, expected %d\n", name, actual, expected);
        g_failed++;
    }
}

static int black_run_at_row(int y, int diameter, int *first_out)
{
    int first = -1;
    int last = -1;
    int count = 0;

    for (int x = 0; x < PUPIL_VIEW_SIZE; x++) {
        if (pupil_scene_color(x, y, diameter, 0.0f, 0.0f) == PUPIL_COLOR_BLACK) {
            if (first < 0) {
                first = x;
            }
            last = x;
            count++;
        }
    }

    if (count > 0 && last - first + 1 != count) {
        return -1;
    }
    if (first_out) {
        *first_out = first;
    }
    return count;
}

static void test_diameter_endpoints(void)
{
    expect_int("d(0)", pupil_diameter(0), PUPIL_DIAMETER_MAX);
    expect_int("d(1250)", pupil_diameter(1250), 32);
    expect_int("d(2500)", pupil_diameter(PUPIL_CYCLE_MS / 2), PUPIL_DIAMETER_MIN);
    expect_int("d(3750)", pupil_diameter(3750), 32);
    expect_int("d(5000)", pupil_diameter(PUPIL_CYCLE_MS), PUPIL_DIAMETER_MAX);
    expect_int("d(7500)", pupil_diameter(PUPIL_CYCLE_MS + PUPIL_CYCLE_MS / 2), PUPIL_DIAMETER_MIN);
    expect_int("d(negative)", pupil_diameter(-10), PUPIL_DIAMETER_MAX);
}

static void test_diameter_shrinks_then_grows(void)
{
    int previous = pupil_diameter(0);
    for (int ms = 100; ms <= PUPIL_CYCLE_MS / 2; ms += 100) {
        int diameter = pupil_diameter(ms);
        if (diameter > previous) {
            fprintf(stderr, "FAIL shrink at %d ms: %d then %d\n", ms, previous, diameter);
            g_failed++;
            return;
        }
        previous = diameter;
    }

    previous = pupil_diameter(PUPIL_CYCLE_MS / 2);
    for (int ms = PUPIL_CYCLE_MS / 2 + 100; ms <= PUPIL_CYCLE_MS; ms += 100) {
        int diameter = pupil_diameter(ms);
        if (diameter < previous) {
            fprintf(stderr, "FAIL grow at %d ms: %d then %d\n", ms, previous, diameter);
            g_failed++;
            return;
        }
        previous = diameter;
    }
}

static void test_wide_eye(void)
{
    int first = -1;

    expect_int("span32", black_run_at_row(120, PUPIL_DIAMETER_MAX, &first), PUPIL_DIAMETER_MAX);
    expect_int("origin32", first, (PUPIL_VIEW_SIZE - PUPIL_DIAMETER_MAX) / 2);
    expect_int("span16", black_run_at_row(120, PUPIL_DIAMETER_MIN, &first), PUPIL_DIAMETER_MIN);
    expect_int("origin16", first, (PUPIL_VIEW_SIZE - PUPIL_DIAMETER_MIN) / 2);
    expect_int("center", pupil_scene_color(120, 120, PUPIL_DIAMETER_MIN, 0.0f, 0.0f), PUPIL_COLOR_BLACK);
    expect_int("highlight", pupil_scene_color(117, 116, PUPIL_DIAMETER_MIN, 0.0f, 0.0f), PUPIL_COLOR_SCLERA);
    expect_int("sclera", pupil_scene_color(120, 70, PUPIL_DIAMETER_MAX, 0.0f, 0.0f), PUPIL_COLOR_SCLERA);
    expect_int("corner", pupil_scene_color(0, 0, PUPIL_DIAMETER_MAX, 0.0f, 0.0f), PUPIL_COLOR_SCLERA);
    expect_int("edge", pupil_scene_color(120, 0, PUPIL_DIAMETER_MAX, 0.0f, 0.0f), PUPIL_COLOR_SCLERA);
    expect_int("shifted", pupil_scene_color(160, 120, PUPIL_DIAMETER_MIN, 40.0f, 0.0f), PUPIL_COLOR_BLACK);
    expect_int("old-center", pupil_scene_color(120, 120, PUPIL_DIAMETER_MIN, 40.0f, 0.0f), PUPIL_COLOR_SCLERA);
}

static void expect_near(const char *name, float actual, float expected)
{
    if (fabsf(actual - expected) > 1.0f) {
        fprintf(stderr, "FAIL %s: got %f, expected %f\n", name, actual, expected);
        g_failed++;
    }
}

static void test_spin_after_two_pulses(void)
{
    pupil_pose_t pose;
    const int64_t rest_start = (int64_t)PUPIL_PULSE_MS * PUPIL_PULSE_COUNT;
    const int64_t spin_start = rest_start + PUPIL_REST_MS + PUPIL_BRIDGE_MS;

    pupil_pose(7500, &pose);
    expect_int("second-min", pose.diameter, PUPIL_DIAMETER_MIN);
    expect_near("second-x", pose.offset_x, 0.0f);
    expect_near("second-y", pose.offset_y, 0.0f);

    pupil_pose(rest_start + 1000, &pose);
    expect_int("rest-size", pose.diameter, PUPIL_DIAMETER_MAX);
    expect_near("rest-x", pose.offset_x, 0.0f);
    expect_near("rest-y", pose.offset_y, 0.0f);

    pupil_pose(spin_start, &pose);
    expect_int("spin-start-size", pose.diameter, PUPIL_DIAMETER_MIN);
    expect_near("spin-start-x", pose.offset_x, 0.0f);
    expect_near("spin-start-y", pose.offset_y, 0.0f);

    pupil_pose(spin_start + PUPIL_SPIN_MS / 12, &pose);
    expect_near("spin-out-x", pose.offset_x, 0.0f);
    if (pose.offset_y <= 2.0f) {
        fprintf(stderr, "FAIL spin-out-y: got %f\n", pose.offset_y);
        g_failed++;
    }

    pupil_pose(spin_start + PUPIL_SPIN_MS / 2, &pose);
    expect_int("spin-size", pose.diameter, PUPIL_DIAMETER_MIN);
    expect_near("spin-mid-x", pose.offset_x, -(float)PUPIL_ORBIT_RADIUS);
    expect_near("spin-mid-y", pose.offset_y, 0.0f);

    pupil_pose(spin_start + (PUPIL_SPIN_MS * 7) / 12, &pose);
    expect_near("spin-back-x", pose.offset_x, 0.0f);
    if (pose.offset_y >= -(float)PUPIL_ORBIT_RADIUS * 0.5f) {
        fprintf(stderr, "FAIL spin-back-y: got %f\n", pose.offset_y);
        g_failed++;
    }

    pupil_pose(spin_start + PUPIL_SPIN_MS - 1, &pose);
    expect_int("spin-end-size", pose.diameter, PUPIL_DIAMETER_MIN);

    pupil_pose(spin_start + PUPIL_SPIN_MS, &pose);
    expect_int("grow-start-size", pose.diameter, PUPIL_DIAMETER_MIN);
    expect_near("grow-start-x", pose.offset_x, 0.0f);
    expect_near("grow-start-y", pose.offset_y, 0.0f);

    pupil_pose(spin_start + PUPIL_SPIN_MS + PUPIL_BRIDGE_MS, &pose);
    expect_int("loop-size", pose.diameter, PUPIL_DIAMETER_MAX);
    expect_near("loop-x", pose.offset_x, 0.0f);
    expect_near("loop-y", pose.offset_y, 0.0f);
}

int main(void)
{
    test_diameter_endpoints();
    test_diameter_shrinks_then_grows();
    test_wide_eye();
    test_spin_after_two_pulses();

    if (g_failed) {
        fprintf(stderr, "%d assertion(s) failed\n", g_failed);
        return 1;
    }
    printf("pupil tests passed\n");
    return 0;
}
