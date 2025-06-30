#define TERMCANVAS_IMPLEMENTATION
#include "screen.h"

int main(void) {
    TermCanvas *screen = init_screen(40, 20, COLOR_WHITE, COLOR_BLACK, ' ');
    
    int i = 0;
    while(i++<10000) print_screen(screen);
    
    screen_shutdown(screen);
    return 0;
}
