#include <cassert>
#include <iostream>

#include <emacs-module.h>

#include "use_gaze.h"

/* `Message' `message` to Emacs. */
void debug(emacs_env *const env, const std::string &message) {
    auto Fmessage = env->intern(env, "message");
    auto Qmessage = env->make_string(env, message.c_str(), message.length());
    env->funcall(env, Fmessage, 1, &Qmessage);
}

/* Calculate the length of `value`. */
intmax_t emacs_string_length(emacs_env *env, emacs_value *value) {
    auto Flength = env->intern(env, "length");
    auto Qlength = env->funcall(env, Flength, 1, value);
    return env->extract_integer(env, Qlength);
}

static emacs_env *_env;
static GazeScreen screen = {1920, 1080};
static ScreenPoint point;

#define ADD_CALLBACK(name)                                                     \
    static emacs_value *_##name;                                               \
    bool wrapped_##name(const Point &p) {                                      \
        point = p.to_screen(screen);                                           \
        emacs_value args[] = {_env->make_integer(_env, point.x),               \
                              _env->make_integer(_env, point.y)};              \
        return _env->is_not_nil(_env, _env->funcall(_env, *_##name, 2, args)); \
    }

#define PARSE_CALLBACK(name, from)                                             \
    if (env->is_not_nil(env, from)) {                                          \
        _##name = &from;                                                       \
    } else {                                                                   \
        _##name = nullptr;                                                     \
    }

#define WRAPPED_CALLBACK(name) _##name ? wrapped_##name : nullptr

ADD_CALLBACK(on_gaze);
ADD_CALLBACK(on_error);
ADD_CALLBACK(on_still);
ADD_CALLBACK(on_exit);

emacs_value gaze_init(emacs_env *env,
                      ptrdiff_t nargs,
                      emacs_value args[],
                      void *) EMACS_NOEXCEPT {
    init();
    return env->intern(env, "nil");
}

emacs_value gaze_cleanup(emacs_env *env,
                         ptrdiff_t nargs,
                         emacs_value args[],
                         void *) EMACS_NOEXCEPT {
    cleanup();
    return env->intern(env, "nil");
}

emacs_value gaze_use(emacs_env *env,
                     ptrdiff_t nargs,
                     emacs_value args[],
                     void *) EMACS_NOEXCEPT {
    debug(env, std::to_string(nargs));

    _env = env;

    PARSE_CALLBACK(on_gaze, args[0]);
    PARSE_CALLBACK(on_error, args[1]);
    PARSE_CALLBACK(on_still, args[2]);
    PARSE_CALLBACK(on_exit, args[3]);
    bool wait_for_callbacks = env->is_not_nil(env, args[4]);
    screen.width = env->extract_integer(env, args[5]);
    screen.height = env->extract_integer(env, args[6]);
    double max_still_milliseconds = env->extract_float(env, args[7]);

    use_gaze(WRAPPED_CALLBACK(on_gaze),
             WRAPPED_CALLBACK(on_error),
             WRAPPED_CALLBACK(on_still),
             WRAPPED_CALLBACK(on_exit),
             wait_for_callbacks,
             &screen,
             max_still_milliseconds);

    return env->intern(env, "nil");
}

emacs_value gaze_subscribe(emacs_env *env,
                           ptrdiff_t nargs,
                           emacs_value args[],
                           void *) EMACS_NOEXCEPT {
    debug(env, std::to_string(nargs));

    _env = env;

    PARSE_CALLBACK(on_gaze, args[0]);
    PARSE_CALLBACK(on_error, args[1]);
    PARSE_CALLBACK(on_still, args[2]);
    PARSE_CALLBACK(on_exit, args[3]);
    bool wait_for_callbacks = env->is_not_nil(env, args[4]);
    screen.width = env->extract_integer(env, args[5]);
    screen.height = env->extract_integer(env, args[6]);
    double max_still_milliseconds = env->extract_float(env, args[7]);

    subscribe_gaze(WRAPPED_CALLBACK(on_gaze),
                   WRAPPED_CALLBACK(on_error),
                   WRAPPED_CALLBACK(on_still),
                   WRAPPED_CALLBACK(on_exit),
                   wait_for_callbacks,
                   &screen,
                   max_still_milliseconds);

    return env->intern(env, "nil");
}

static void bind_function(emacs_env *env, const char *name, emacs_value Sfun) {
    /* Convert the strings to symbols by interning them */
    auto Qfset = env->intern(env, "fset");
    auto Qsym = env->intern(env, name);

    /* Prepare the arguments array */
    emacs_value args[] = {Qsym, Sfun};

    /* Make the call (2 == nb of arguments) */
    env->funcall(env, Qfset, 2, args);
}

/* Provide FEATURE to Emacs.  */
static void provide(emacs_env *env, const char *feature) {
    /* call 'provide' with FEATURE converted to a symbol */

    auto Qfeat = env->intern(env, feature);
    auto Qprovide = env->intern(env, "provide");
    emacs_value args[] = {Qfeat};

    env->funcall(env, Qprovide, 1, args);
}

#ifdef __cplusplus
extern "C" {
#endif
int plugin_is_GPL_compatible;

int emacs_module_init(struct emacs_runtime *ert) EMACS_NOEXCEPT {
    auto *env = ert->get_environment(ert);

    auto init_gaze_function =
        env->make_function(env, 0, 0, gaze_init, "", nullptr);
    bind_function(env, "gaze--init", init_gaze_function);

    auto subscribe_gaze_function = env->make_function(
        env,
        8,
        8,
        gaze_subscribe,
        "Subscrobe to gaze stream, must be called after gaze--init",
        nullptr);
    bind_function(env, "gaze--subscribe", subscribe_gaze_function);

    auto cleanup_gaze_function =
        env->make_function(env, 0, 0, gaze_cleanup, "", nullptr);
    bind_function(env, "gaze--clenaup", cleanup_gaze_function);

    auto use_gaze_function =
        env->make_function(env,
                           8,
                           8,
                           gaze_use,
                           "gaze--init, gaze--subscribe and gaze--cleanup",
                           nullptr);
    bind_function(env, "gaze--use", use_gaze_function);

    provide(env, "use-gaze");

    return 0;
}
#ifdef __cplusplus
}
#endif
