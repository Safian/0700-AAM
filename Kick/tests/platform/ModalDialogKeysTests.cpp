// Host test for apps/kick/hardware/modal_dialog_keys.h. g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
#include "../../apps/kick/hardware/modal_dialog_keys.h"
#include <cstdio>
#include <initializer_list>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++g_fail; } } while (0)

int main()
{
    using namespace kick_hw;
    CHECK(timingCorrectDialogAction(HW_BTN_ENTER, false) == ModalDialogAction::Apply);
    CHECK(timingCorrectDialogAction(HW_BTN_ENTER, true)  == ModalDialogAction::Apply);
    CHECK(timingCorrectDialogAction(HW_BTN_STOP, false)  == ModalDialogAction::Apply);
    CHECK(timingCorrectDialogAction(HW_BTN_STOP, true)   == ModalDialogAction::Cancel);
    CHECK(timingCorrectDialogAction(HW_BTN_MAIN, false)  == ModalDialogAction::Cancel);
    CHECK(timingCorrectDialogAction(HW_BTN_MAIN, true)   == ModalDialogAction::Cancel);
    CHECK(timingCorrectDialogAction(HW_BTN_SHIFT, false) == ModalDialogAction::None);
    CHECK(timingCorrectDialogAction(HW_BTN_TC, false)    == ModalDialogAction::None);
    CHECK(timingCorrectDialogAction(HW_BTN_PLAY, false)  == ModalDialogAction::None);

    // Any dialog: button x shift x has Apply.
    for (bool shift : { false, true }) {
        CHECK(modalDialogAction(HW_BTN_ENTER, shift, true)  == ModalDialogAction::Apply);
        CHECK(modalDialogAction(HW_BTN_ENTER, shift, false) == ModalDialogAction::Cancel); // nothing to apply: just close
        CHECK(modalDialogAction(HW_BTN_MAIN, shift, true)   == ModalDialogAction::Cancel);
        CHECK(modalDialogAction(HW_BTN_MAIN, shift, false)  == ModalDialogAction::Cancel);
        CHECK(modalDialogAction(HW_BTN_PLAY, shift, true)   == ModalDialogAction::None);
        CHECK(modalDialogAction(HW_BTN_MENU, shift, true)   == ModalDialogAction::None);
        CHECK(modalDialogAction(HW_BTN_SHIFT, shift, true)  == ModalDialogAction::None);
    }
    CHECK(modalDialogAction(HW_BTN_STOP, false, true)  == ModalDialogAction::Apply);
    CHECK(modalDialogAction(HW_BTN_STOP, true, true)   == ModalDialogAction::Cancel);
    CHECK(modalDialogAction(HW_BTN_STOP, false, false) == ModalDialogAction::Cancel);
    CHECK(modalDialogAction(HW_BTN_STOP, true, false)  == ModalDialogAction::Cancel);
    // The TC mapping is the has-Apply case of the general one.
    for (int b = 0; b < NUM_HW_BUTTONS; ++b)
        for (bool shift : { false, true })
            CHECK(timingCorrectDialogAction((HardwareButton)b, shift) == modalDialogAction((HardwareButton)b, shift, true));

    // While a dialog is open: transport + dial keys work, screen switchers / edit buttons are swallowed.
    for (HardwareButton b : { HW_BTN_PLAY, HW_BTN_PLAY_START, HW_BTN_STOP, HW_BTN_REC, HW_BTN_OVERDUB, HW_BTN_TAP_TEMPO, HW_BTN_PLUS, HW_BTN_MINUS,
                              HW_BTN_ENTER, HW_BTN_CURSOR_UP, HW_BTN_CURSOR_DOWN, HW_BTN_CURSOR_LEFT, HW_BTN_CURSOR_RIGHT, HW_BTN_SHIFT })
        CHECK(modalAllowsButton(b));
    for (HardwareButton b : { HW_BTN_MENU, HW_BTN_MAIN, HW_BTN_GRID, HW_BTN_BROWSE, HW_BTN_TRACK_MIX, HW_BTN_PAD_MIX, HW_BTN_STEP_SEQ, HW_BTN_SAMPLER,
                              HW_BTN_PROG_EDIT, HW_BTN_UNDO, HW_BTN_ERASE, HW_BTN_COPY, HW_BTN_SOUNDS, HW_BTN_TC, HW_BTN_NONE, HW_BTN_BANK_A, HW_BTN_LOCATE })
        CHECK(!modalAllowsButton(b));
    // Every button that can close a dialog is either allowed or MAIN (which the dialog logic handles before the policy).
    for (int b = 0; b < NUM_HW_BUTTONS; ++b)
        if (modalDialogAction((HardwareButton)b, false, true) != ModalDialogAction::None && (HardwareButton)b != HW_BTN_MAIN)
            CHECK(modalAllowsButton((HardwareButton)b));
    if (g_fail) return 1;
    std::printf("OK\n");
    return 0;
}
