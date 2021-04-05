#ifndef USE_GAZE_H
#define USE_GAZE_H

#include <assert.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdio.h>

#include <tobii/tobii.h>
#include <tobii/tobii_config.h>
#include <tobii/tobii_streams.h>

struct GazeScreen {
    int width;
    int height;
};

struct ScreenPoint {
    int x;
    int y;

    bool is_same(const ScreenPoint &other) const {
        return (x - other.x) == 0 && (y - other.y) == 0;
    }
};

struct Point {
    double x;
    double y;

    ScreenPoint to_screen(const GazeScreen &screen) const {
        return {(int)std::round(x * screen.width),
                (int)std::round(y * screen.height)};
    }

    bool is_same(const Point &other) const {
        return std::abs(x - other.x) < 1e-5 && std::abs(y - other.y) < 1e-5;
    }

    bool is_same_in_screen(const Point &other, const GazeScreen &screen) const {
        return to_screen(screen).is_same(other.to_screen(screen));
    }
};

typedef bool(GazeCallback)(const Point &);

static Point last_point = {0, 0};
static Point current_point = {0, 0};
static bool point_updated = false;
static bool point_ever_updated = false;

static GazeScreen *_screen = nullptr;
static std::chrono::steady_clock::time_point last_gazed_time =
    std::chrono::steady_clock::now();
static std::chrono::steady_clock::time_point current_gazed_time =
    std::chrono::steady_clock::now();
static std::chrono::duration<double, std::milli> gaze_still_duration;
static double _max_still_milliseconds = -1;
static bool in_progress = false;
static bool aborted = false;

static tobii_api_t *api;
static tobii_error_t error;
static tobii_device_t *device;

void url_receiver(char const *url, void *user_data) {
    char *buffer = (char *)user_data;
    if (*buffer != '\0')
        return;  // only keep first value

    if (strlen(url) < 256) {
        strcpy(buffer, url);
    }
}

bool cleanup() {
    if (api == nullptr || device == nullptr) {
        return true;
    }

    tobii_error_t error = tobii_gaze_point_unsubscribe(device);
    if (error != TOBII_ERROR_NO_ERROR) {
        std::cout << "tobii_gaze_point_unsubscribe error" << std::endl;
        return false;
    };

    error = tobii_device_destroy(device);
    if (error != TOBII_ERROR_NO_ERROR) {
        std::cout << "tobii_device_destroy error" << std::endl;
        return false;
    };

    error = tobii_api_destroy(api);
    if (error != TOBII_ERROR_NO_ERROR) {
        std::cout << "tobii_api_destroy error" << std::endl;
        return false;
    };

    api = nullptr;
    device = nullptr;
    in_progress = false;

    std::cout << "Cleaned Up Tobii!" << std::endl << std::endl;

    return true;
}

void optional_invoke_callback(GazeCallback callback,
                              const Point &point,
                              bool set_aborted = true) {
    if (callback == nullptr) {
        return;
    }

    if (set_aborted) {
        aborted = callback(point);
    } else {
        callback(point);
    }
}

bool init() {
    if (in_progress) {
        return false;
    }

    error = tobii_api_create(&api, NULL, NULL);
    if (error != TOBII_ERROR_NO_ERROR) {
        std::cout << "tobii_api_create error" << std::endl;
        return false;
    };

    // Enumerate devices to find connected eye trackers, keep the first
    char url[256] = {0};
    error = tobii_enumerate_local_device_urls(api, url_receiver, url);
    if (error != TOBII_ERROR_NO_ERROR) {
        std::cout << "tobii_enumerate_local_device_urls error" << std::endl;
        return false;
    };

    if (*url == '\0') {
        std::cout << "Error: No device found" << std::endl;
        return false;
    }

    // Connect to the first tracker found
    error = tobii_device_create(api, url, &device);
    if (error != TOBII_ERROR_NO_ERROR) {
        std::cout << "tobii_device_create error" << std::endl;
        return false;
    };

    error = tobii_gaze_point_subscribe(
        device,
        [](tobii_gaze_point_t const *gaze_point, void * /* user_data */) {
            if (gaze_point->validity == TOBII_VALIDITY_VALID) {
                current_point.x = gaze_point->position_xy[0];
                current_point.y = gaze_point->position_xy[1];
                point_updated =
                    _screen
                        ? !current_point.is_same_in_screen(last_point, *_screen)
                        : !current_point.is_same(last_point);
                if (_max_still_milliseconds > 0 && point_updated) {
                    current_gazed_time = std::chrono::steady_clock::now();
                }
            }
        },
        0);
    if (error != TOBII_ERROR_NO_ERROR) {
        std::cout << "tobii_gaze_point_subscribe error" << std::endl;
        return false;
    };

    return true;
}

bool subscribe_gaze(GazeCallback on_gaze = nullptr,
                    GazeCallback on_error = nullptr,
                    GazeCallback on_still = nullptr,
                    GazeCallback on_exit = nullptr,
                    bool wait_for_callbacks = false,
                    GazeScreen *const screen = nullptr,
                    double max_still_milliseconds = -1) {
    if (!on_gaze && !on_error && !on_still && !on_exit) {
        std::cout << "No callbacks set!" << std::endl;
        return false;
    }
    if (in_progress) {
        return false;
    }

    _screen = screen;
    _max_still_milliseconds = max_still_milliseconds;

    last_point.x = 0;
    last_point.y = 0;
    current_point.x = 0;
    current_point.y = 0;
    point_ever_updated = false;
    in_progress = true;
    aborted = false;

    while (!aborted) {
        if (wait_for_callbacks) {
            error = tobii_wait_for_callbacks(1, &device);
            if (error != TOBII_ERROR_NO_ERROR &&
                error != TOBII_ERROR_TIMED_OUT) {
                std::cout << "tobii_wait_for_callbacks error" << std::endl;
                optional_invoke_callback(on_error, last_point, false);
                aborted = true;
                break;
            };
        }

        point_updated = false;
        error = tobii_device_process_callbacks(device);
        if (error != TOBII_ERROR_NO_ERROR) {
            std::cout << "tobii_device_process_callbacks error" << std::endl;
            optional_invoke_callback(on_error, last_point, false);
            aborted = true;
            break;
        };

        if (point_updated) {
            // std::cout << "Gaze: " << current_point.x << ", " <<
            // current_point.y
            //           << std::endl;
            optional_invoke_callback(on_gaze, current_point);
            last_point.x = current_point.x;
            last_point.y = current_point.y;
            last_gazed_time = current_gazed_time;
            point_ever_updated = true;
        } else {
            if (point_ever_updated && max_still_milliseconds > 0) {
                gaze_still_duration =
                    std::chrono::steady_clock::now() - last_gazed_time;
                if (gaze_still_duration.count() >= max_still_milliseconds) {
                    optional_invoke_callback(on_still, last_point);
                } else {
                    optional_invoke_callback(on_error, last_point);
                }
            } else {
                optional_invoke_callback(on_error, last_point);
            }
        }
    }

    optional_invoke_callback(on_exit, last_point, false);
    in_progress = false;

    return true;
}

bool use_gaze(GazeCallback on_gaze = nullptr,
              GazeCallback on_error = nullptr,
              GazeCallback on_still = nullptr,
              GazeCallback on_exit = nullptr,
              bool wait_for_callbacks = false,
              GazeScreen *const screen = nullptr,
              double max_still_milliseconds = -1) {
    bool ret;
    ret = init();
    if (!ret) {
        return false;
    }

    ret = subscribe_gaze(on_gaze,
                         on_error,
                         on_still,
                         nullptr,
                         wait_for_callbacks,
                         screen,
                         max_still_milliseconds);

    ret = cleanup() && ret;
    optional_invoke_callback(on_exit, last_point, false);

    return ret;
}

#endif /* USE_GAZE_H */
