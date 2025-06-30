#define TERMCANVAS_IMPLEMENTATION
#include "screen.h"

int main(void) {
    TermCanvas *screen = init_screen(100, 100, COLOR_WHITE, COLOR_BLACK, ' ');
    print_screen(screen);
    screen_shutdown(screen);
    return 0;
}