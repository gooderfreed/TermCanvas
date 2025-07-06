#define TERMCANVAS_IMPLEMENTATION
#include "termcanvas.h" 

#include <stdio.h>

int main(void) {
    TermCanvas *screen = tc_create(100, 10, L'+', COLOR_BLACK, COLOR_WHITE);
    if (!screen) {
        printf("Failed to create canvas.\n");
        return 1;
    }

    int i = 0;
    while (i < 10000000) {
        // tc_fill_area(screen, 0, 0, screen->height, screen->width, L' ', 
        //              COLOR_BLACK, COLOR_WHITE, Effect_None);
        tc_show(screen);
        // getchar();
        i++;
    }

    tc_destroy(screen);
    
    return 0;
}