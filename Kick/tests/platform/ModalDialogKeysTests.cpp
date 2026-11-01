// Host test for apps/kick/hardware/modal_dialog_keys.h. g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
#include "../../apps/kick/hardware/modal_dialog_keys.h"
#include <cstdio>

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
    if (g_fail) return 1;
    std::printf("OK\n");
    return 0;
}
