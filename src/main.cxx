#include "use_gaze.h"

static int n = 1000;

bool callback(const Point &p) {
    printf("Gaze point: %f, %f\n", p.x, p.y);
    n -= 1;
    return n == 0;
}

int main(int argc, char **argv) {
    use_gaze(callback, nullptr, nullptr, nullptr, true);
    return 0;
}
