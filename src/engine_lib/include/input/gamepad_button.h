#pragma once

enum te_gamepad_button {
    TE_GB_LEFT, /* one of the 4 buttons on the right side of a gamepad, X button */
                /* on xbox, square on sony gamepad and so on */
    TE_GB_UP,
    TE_GB_RIGHT,
    TE_GB_DOWN,
    TE_GB_START,
    TE_GB_BACK,
    TE_GB_DPAD_LEFT,
    TE_GB_DPAD_UP,
    TE_GB_DPAD_RIGHT,
    TE_GB_DPAD_DOWN,
    TE_GB_LEFT_STICK,
    TE_GB_RIGHT_STICK,
    TE_GB_LEFT_SHOULDER,
    TE_GB_RIGHT_SHOULDER
};

enum te_gamepad_axis {
    TE_GA_RIGHT_TRIGGER,
    TE_GA_LEFT_TRIGGER,
    TE_GA_RIGHT_STICK_X,
    TE_GA_RIGHT_STICK_Y,
    TE_GA_LEFT_STICK_X,
    TE_GA_LEFT_STICK_Y
};
