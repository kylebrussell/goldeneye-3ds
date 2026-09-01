#include "ge_original_input.h"

#include <assert.h>

#include <ultra64.h>

int main(void)
{
    GeOriginalInputSample sample = {0};
    GeOriginalBondInputFrame frame;

    ge_original_input_init();
    sample.move_x = 0.5f;
    sample.move_y = -1.0f;
    sample.buttons = A_BUTTON | Z_TRIG;
    ge_original_input_tick(&sample);
    ge_original_input_read_bond_frame(B_BUTTON, &frame);

    assert(frame.stick_x == 30);
    assert(frame.stick_y == -60);
    assert(frame.buttons == (A_BUTTON | Z_TRIG));
    assert(frame.oldbuttons == B_BUTTON);
    return 0;
}
