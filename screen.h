#ifndef TERMCANVAS_H
#define TERMCANVAS_H

#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <locale.h>

#include "coords.h"
#include "color.h"


/*
 * Terminal control macros
 * Basic terminal manipulation commands
 */

// -----------------------------------------------------------------------------
//  Constants and Macros
// -----------------------------------------------------------------------------
#define MAX_ANSI_LENGTH 50 // Define a reasonable maximum for ANSI sequences


// Terminal control macros
#define tc_clear()       wprintf(L"\033[H\033[J")          // Clear the entire screen
#define tc_gotoxy(x, y)  wprintf(L"\033[%d;%dH", (y), (x)) // Position cursor
#define tc_hide_cursor() wprintf(L"\033[?25l")             // Hide the cursor
#define tc_show_cursor() wprintf(L"\033[?25h")             // Show the cursor


typedef enum TcEffect TcEffect;
typedef struct TcPixel TcPixel;
typedef struct TermCanvas TermCanvas;

// -----------------------------------------------------------------------------
//  Type Definitions
// -----------------------------------------------------------------------------
/*
 * Text effects (bold, italic, etc.).
 */
enum TcEffect {
    Effect_None      = 0,
    Effect_Bold      = 1,  // Bold/bright
    Effect_Italic    = 3,  // Italic (not widely supported)
    Effect_Underline = 4,  // Underline
    Effect_Blink     = 5,  // Blink (rarely supported, can be annoying)
    Effect_Reverse   = 7,  // Reverse video (swap foreground and background)
    Effect_Conceal   = 8,  // Conceal (hide text, rarely supported)
};

/*
 * Represents a single pixel on the screen.
 */
struct TcPixel {
    Color    background; // Background color
    Color    foreground; // Foreground color
    wchar_t  symbol;     // Character to display
    TcEffect effect;     // Text effect (bold, italic, etc.)
};

/*
 * Terminal color modes
 * Basic 8/16 colors
 * 256-color mode
 * TrueColor (RGB) mode
 */
typedef enum {
    Color_Base, // Basic 8/16 colors
    Color_256,  // 256-color mode
    Color_RGB   // TrueColor (RGB) mode
} TerminalMode;


/*
 * Screen structure
 * Represents the game screen with dimensions, pixel data, and a render buffer.
 */
struct TermCanvas {
    int height;        // Screen height in pixels
    int width;         // Screen width in pixels
    TcPixel **pixels;    // 2D array of pixel data
    
    wchar_t *buffer;   // Render buffer for output
    int buffer_size;   // Size of the render buffer

    TerminalMode mode; // Terminal color mode
};


/*
* Screen functions
* Screen manipulation and drawing
*/

// Screen management
#ifdef USE_ARENA
TermCanvas *tc_create(Arena *arena, int width, int height, wchar_t symbol, Color foreground, Color background);
#else
TermCanvas *tc_create(int width, int height, wchar_t symbol, Color foreground, Color background);
#endif

void tc_destroy(TermCanvas *canvas);
void tc_show(const TermCanvas *canvas);

// Drawing functions
void tc_draw_borders(TermCanvas *canvas, int x, int y, int width, int height, const wchar_t *borders, Color foreground, Color background, TcEffect effect);
void tc_draw_hline(TermCanvas   *canvas, int x, int y,                        const wchar_t *borders, Color foreground, Color background, TcEffect effect);
void tc_draw_wline(TermCanvas   *canvas, int x, int y,                        const wchar_t *borders, Color foreground, Color background, TcEffect effect);
void tc_fill_area(TermCanvas    *canvas, int x, int y, int width, int height, wchar_t symbol,         Color foreground, Color background, TcEffect effect);
void tc_put_pixel(TermCanvas    *canvas, int x, int y,                        wchar_t symbol,         Color foreground, Color background, TcEffect effect);
void tc_draw_text(TermCanvas    *canvas, int x, int y,                        const char *text,       Color foreground, Color background, TcEffect effect);
void tc_draw_wtext(TermCanvas   *canvas, int x, int y,                        const wchar_t *text,    Color foreground, Color background, TcEffect effect);


// -----------------------------------------------------------------------------
//  Macros for Pixel Manipulation
// -----------------------------------------------------------------------------
#define SET_PIXEL_FOREGROUND(pixel, color)                 \
    if (!is_none(color)) (pixel)->foreground = (color);

#define SET_PIXEL_BACKGROUND(pixel, color)                 \
    if (!is_none(color)) (pixel)->background = (color);

#define SET_PIXEL_COLOR(pixel, config)                     \
    SET_PIXEL_FOREGROUND(pixel, (config).foreground)       \
    SET_PIXEL_BACKGROUND(pixel, (config).background)
    
#define SET_PIXEL(pixel_target, pixel_source)              \
    do {                                                   \
        SET_PIXEL_COLOR(pixel_target, pixel_source)        \
        (pixel_target)->symbol = (pixel_source).symbol;    \
    } while(0);



#ifdef TERMCANVAS_IMPLEMENTATION
// -----------------------------------------------------------------------------
//  ANSI Escape Code Generation
// -----------------------------------------------------------------------------
/*
 * Generates an ANSI escape sequence for text effects.
 * It formats the text effect into the ANSI escape sequence.
 */
static char* get_effect_ansi(TcEffect effect) {
    static char ansi_str[16];             // Buffer for the ANSI escape sequence
    if (effect == Effect_None) return ""; // Return empty string if no effect

    snprintf(ansi_str, sizeof(ansi_str), "\033[%dm", (int)effect);
    return ansi_str;
}



// -----------------------------------------------------------------------------
//  Terminal Capability Detection (tput)
// -----------------------------------------------------------------------------

// static inline int is_terminal(void) {
//     return isatty(STDOUT_FILENO);
// }

/*
 * Checks if the 'tput' command exists in the system.
 * It checks if the 'tput' command exists in the system by trying to access it.
 */
static inline int tput_exists(void) {
    return access("/usr/bin/tput", X_OK) == 0 || access("/bin/tput", X_OK) == 0;
}

/*
 * Executes a 'tput' command and returns the integer result.
 */
static int tput_command(const char *command) {
    FILE *fp = popen(command, "r");
    if (!fp) return -1; // Return -1 on failure

    int result = -1;
    if (fscanf(fp, "%d", &result) != 1) { // Check for fscanf errors
        result = -1;
    }
    pclose(fp);

    return result;
}

/*
 * Retrieves and caches the number of colors supported by the terminal.
 * It checks if the tput command exists and uses it to get the number of colors,
 * caching the result to avoid repeated calls.
 */
static int get_cached_tput_colors(void) {
    static int cached_colors = -2; // -2: uninitialized, -1: tput failed

    if (cached_colors == -2) {
        cached_colors = tput_exists() ? tput_command("tput colors 2>/dev/null") : -1;
    }

    return cached_colors;
}

/*
 * Checks if the terminal supports RGB (TrueColor) mode.
 * It checks if the COLORTERM environment variable matches common terminal types that often support TrueColor,
 * and uses tput if available to check for TrueColor support.
 */
static int supports_rgb(int tput_colors) {
    const char *colorterm = getenv("COLORTERM");
    if (colorterm && strstr(colorterm, "truecolor")) return 1;

    const char *term = getenv("TERM");
    if (term && (strstr(term, "truecolor")    || strstr(term, "direct") ||
                 strstr(term, "xterm-direct") || strstr(term, "xterm-truecolor"))) {
        return 1;
    }
    if (tput_colors > 0) {
        return tput_colors >= (1 << 24); // TrueColor requires at least 2^24 colors
    }
    return 0;
}

/*
 * Checks if the terminal supports 256 colors.
 * It checks if the TERM environment variable matches common terminal types that often support 256 colors,
 * and uses tput if available to check for 256-color support.
 */
static int supports_256(int tput_colors) {
    const char *term = getenv("TERM");
    if (term && strstr(term, "256color")) return 1;

    if (term) {
        // Check for common terminal types that often support 256 colors
        if (strstr(term, "xterm")  || strstr(term, "rxvt")  || strstr(term, "linux")     ||
            strstr(term, "screen") || strstr(term, "tmux")  || strstr(term, "vt100")     ||
            strstr(term, "vt220")  || strstr(term, "ansi")  || strstr(term, "konsole")   ||
            strstr(term, "Eterm")  || strstr(term, "gnome") || strstr(term, "alacritty") ||
            strstr(term, "st")     || strstr(term, "foot")  || strstr(term, "kitty")) {
            if (tput_colors > 0) return tput_colors >= 256; // Use tput if available
            return 1; // Assume 256-color support if TERM matches
        }
    }

    if (tput_colors >= 256) return 1; // Fallback to tput result

    return 0;
}

/*
 * Determines the terminal's color mode (Base, 256, or RGB).
 * It checks if the terminal supports TrueColor or 256-color mode,
 * and returns the appropriate TerminalMode enum value.
 */
static TerminalMode get_terminal_mode(void) {
    int tput_colors = get_cached_tput_colors();

    if (supports_rgb(tput_colors)) return Color_RGB;
    if (supports_256(tput_colors)) return Color_256;

    return Color_Base;
}


// -----------------------------------------------------------------------------
//  ANSI Escape Code Generation (Color Conversion)
// -----------------------------------------------------------------------------

/*
 * Converts RGB color to an ANSI escape sequence for TrueColor terminals.
 * It formats the RGB values into the ANSI escape sequence for TrueColor terminals.
 */
static char* rgb_to_ansi(Color fg_color, Color bg_color) {
    static char ansi_str[128]; // Buffer for the ANSI escape sequence

    snprintf(ansi_str, sizeof(ansi_str), "\033[38;2;%d;%d;%d;48;2;%d;%d;%dm",
             get_red(fg_color), get_green(fg_color), get_blue(fg_color),
             get_red(bg_color), get_green(bg_color), get_blue(bg_color));

    return ansi_str;
}

/*
 * Converts RGB color to the nearest index in the 256-color palette.
 * It first checks for grayscale colors and maps them to the corresponding index,
 * then maps the RGB values to the 6x6x6 color cube and returns the corresponding index.
 */
static int rgb_to_256_index(Color color) {
    int r = get_red(color);
    int g = get_green(color);
    int b = get_blue(color);

    // Check for grayscale colors first
    int gray_index = -1;
    float gray_step = 255.0f / 24.0f;
    for (int i = 0; i < 24; i++) {
        int gray_val = 8 + i * 10;

        if (fabsf((float)(r - gray_val)) <= gray_step &&
            fabsf((float)(g - gray_val)) <= gray_step &&
            fabsf((float)(b - gray_val)) <= gray_step) {
            gray_index = 232 + i;
            break;
        }
    }
    if (gray_index >= 0) return gray_index;

    // If not grayscale, map to the 6x6x6 color cube
    int r_6 = (int)lroundf((float)r / 255.0f * 5.0f);
    int g_6 = (int)lroundf((float)g / 255.0f * 5.0f);
    int b_6 = (int)lroundf((float)b / 255.0f * 5.0f);

    return 16 + 36 * r_6 + 6 * g_6 + b_6;
}

/*
 * Converts RGB colors to the nearest ANSI escape sequence for 256-color terminals.
 * It first converts the RGB values to the nearest index in the 256-color palette,
 * then returns the corresponding ANSI escape sequence.
 */
static char* rgb_to_ansi_256(Color fg_color, Color bg_color) {
    static char ansi_str[64]; // Buffer for the ANSI escape sequence

    snprintf(ansi_str, sizeof(ansi_str), "\033[38;5;%d;48;5;%dm",
             rgb_to_256_index(fg_color), rgb_to_256_index(bg_color));

    return ansi_str;
}

/*
 * Converts RGB colors to the nearest ANSI escape sequence for basic 8/16 color terminals.
 * It first calculates the Euclidean distance between the RGB values and the basic colors,
 * then selects the closest color and returns the corresponding ANSI escape sequence.
 */
static char *rgb_to_ansi_base(Color fg_color, Color bg_color) {
    static char ansi_str[32];  // Buffer for the ANSI escape sequence
    int r_fg = get_red(fg_color);
    int g_fg = get_green(fg_color);
    int b_fg = get_blue(fg_color);

    int r_bg = get_red(bg_color);
    int g_bg = get_green(bg_color);
    int b_bg = get_blue(bg_color);

    // Find the closest basic color index for foreground
    int index_fg = 0;
    float min_dist_fg = 1e9f; // Initialize with a large value

    // Find the closest basic color index for background
    int index_bg = 0;
    float min_dist_bg = 1e9f;

    // Standard 8 colors (black, red, green, yellow, blue, magenta, cyan, white)
    int basic_colors[8][3] = {
        {0, 0, 0},      // Black
        {128, 0, 0},    // Red
        {0, 128, 0},    // Green
        {128, 128, 0},  // Yellow
        {0, 0, 128},    // Blue
        {128, 0, 128},  // Magenta
        {0, 128, 128},  // Cyan
        {192, 192, 192} // Light Gray (NOT White)
    };

    // Bright 8 colors (corresponding bright versions)
    int bright_colors[8][3] = {
        {128, 128, 128},  // Dark Gray (Bright Black)
        {255, 0, 0},      // Bright Red
        {0, 255, 0},      // Bright Green
        {255, 255, 0},    // Bright Yellow
        {0, 0, 255},      // Bright Blue
        {255, 0, 255},    // Bright Magenta
        {0, 255, 255},    // Bright Cyan
        {255, 255, 255}   // Bright White
    };

    // Calculate Euclidean distance to find the closest basic color (foreground)
    for (int i = 0; i < 8; i++) {
        float dist = (float)sqrt(pow(r_fg - basic_colors[i][0], 2) +
                                 pow(g_fg - basic_colors[i][1], 2) +
                                 pow(b_fg - basic_colors[i][2], 2));
        if (dist < min_dist_fg) {
            min_dist_fg = dist;
            index_fg = i;
        }
        // Calculate Euclidean distance for background
        dist = (float)sqrt(pow(r_bg - basic_colors[i][0], 2) +
                           pow(g_bg - basic_colors[i][1], 2) +
                           pow(b_bg - basic_colors[i][2], 2));
        if (dist < min_dist_bg) {
            min_dist_bg = dist;
            index_bg = i;
        }
    }

    // Calculate Euclidean distance to find the closest bright color (foreground)
    for (int i = 0; i < 8; i++) {
        float dist = (float)sqrt(pow(r_fg - bright_colors[i][0], 2) +
                                 pow(g_fg - bright_colors[i][1], 2) +
                                 pow(b_fg - bright_colors[i][2], 2));
        if (dist < min_dist_fg) {
            min_dist_fg = dist;
            index_fg = i + 8; // Offset index for bright colors
        }
        //background
        dist = (float)sqrt(pow(r_bg - bright_colors[i][0], 2) +
                           pow(g_bg - bright_colors[i][1], 2) +
                           pow(b_bg - bright_colors[i][2], 2));
        if (dist < min_dist_bg) {
            min_dist_bg = dist;
            index_bg = i + 8;
        }
    }
    // Determine ANSI color codes based on foreground and background indices
    int fg_code = (index_fg < 8) ? (30 + index_fg) : (90 + (index_fg - 8));  // 30-37 or 90-97
    int bg_code = (index_bg < 8) ? (40 + index_bg) : (100 + (index_bg - 8)); // 40-47 or 100-107

    snprintf(ansi_str, sizeof(ansi_str), "\033[%d;%dm", fg_code, bg_code);
    return ansi_str;
}

/*
 * Selects the appropriate ANSI escape sequence based on the terminal color mode.
 * Uses different conversion functions based on the color mode:
 * - Color_RGB: Uses rgb_to_ansi for TrueColor terminals
 * - Color_256: Uses rgb_to_ansi_256 for 256-color terminals
 * - Color_Base: Uses rgb_to_ansi_base for basic 8/16 color terminals
 */
static char* get_color_ansi(Color fg_color, Color bg_color, TerminalMode mode) {
    switch (mode) {
        case Color_RGB:   return rgb_to_ansi(fg_color, bg_color);
        case Color_256:   return rgb_to_ansi_256(fg_color, bg_color);
        case Color_Base:  return rgb_to_ansi_base(fg_color, bg_color);
        default:          return ""; // Should not happen, but return empty string for safety
    }
}



// -----------------------------------------------------------------------------
//  Screen Initialization and Shutdown
// -----------------------------------------------------------------------------
/*
 * Initializes an empty screen.
 * Creates the screen structure with cleared buffers and sets the terminal mode.
 */
#ifdef USE_ARENA
TermCanvas *tc_create(Arena *arena, int width, int height, wchar_t symbol, Color foreground, Color background) {
    TermCanvas *canvas = (TermCanvas *)arena_alloc(arena, sizeof(TermCanvas));
#else
TermCanvas *tc_create(int width, int height, wchar_t symbol, Color foreground, Color background) {
    TermCanvas *canvas = (TermCanvas *)malloc(sizeof(TermCanvas));
#endif
    canvas->width = width;
    canvas->height = height;
    canvas->mode = get_terminal_mode();
    
    #ifdef USE_ARENA
    void *blob = arena_alloc(arena, (size_t)(width * height) * sizeof(Pixel) + sizeof(Pixel *) * (size_t)(height));
    #else
    void *blob = malloc((size_t)(width * height) * sizeof(TcPixel) + sizeof(TcPixel *) * (size_t)(height));
    #endif

    canvas->pixels = (TcPixel **)blob;

    TcPixel pixel = (TcPixel) {background, foreground, symbol, Effect_None};
    for (int i = 0; i < height; i++) {
        canvas->pixels[i] = (TcPixel *)(void *)((char *)blob + sizeof(TcPixel *) * (size_t)(height) + (size_t)(i) * (size_t)(width) * sizeof(TcPixel));
        for (int j = 0; j < width; j++) {
            canvas->pixels[i][j] = pixel;
        }
    }
    
    canvas->buffer_size = ((15) * canvas->width * canvas->height + 8 + canvas->height) / 20;

    #ifdef USE_ARENA
    canvas->buffer = (wchar_t *)arena_alloc(arena, sizeof(wchar_t) * (size_t)(canvas->buffer_size));
    #else
    canvas->buffer = (wchar_t *)malloc(sizeof(wchar_t) * (size_t)(canvas->buffer_size));
    #endif

    setlocale(LC_ALL, "");
    tc_hide_cursor();
    tc_clear();

    return canvas;
}

/*
 * Shuts down the screen.
 * Restores terminal settings and clears the screen.
 */
void tc_destroy(TermCanvas *canvas) {
    if (!canvas) return;

    #ifdef USE_ARENA
    arena_free_block(canvas->buffer);  // free buffer
    arena_free_block(canvas->pixels);  // free pixels
    #else
    free(canvas->buffer);  // free buffer
    free(canvas->pixels);  // free pixels
    #endif

    tc_clear();           // Clear the canvas
    tc_show_cursor();     // Show the cursor
}


// -----------------------------------------------------------------------------
//  Drawing Functions
// -----------------------------------------------------------------------------
/*
 * Add borders to specified area of screen
 * Creates a border using provided border characters
 */
void tc_draw_borders(TermCanvas *canvas, int x, int y, int width, int height, const wchar_t *borders, Color foreground, Color background, TcEffect effect) {
    if (!canvas || !borders) return; // Check for NULL pointers
    if (y < 0 || x < 0 || height <= 0 || width <= 0) return; //basic checks
    if (y + height > canvas->height || x + width > canvas->width) return;

    // Draw horizontal borders (top and bottom)
    TcPixel horizontal_pixel = (TcPixel) {background, foreground, borders[0], effect};
    for (int i = 0; i < width; ++i) {
        SET_PIXEL(&canvas->pixels[y][x + i], horizontal_pixel);
        SET_PIXEL(&canvas->pixels[y + height - 1][x + i], horizontal_pixel);
    }

    // Draw vertical borders (left and right)
    TcPixel vertical_pixel = (TcPixel) {background, foreground, borders[1], effect};
    for (int i = 0; i < height; ++i) {
        SET_PIXEL(&canvas->pixels[y + i][x], vertical_pixel);
        SET_PIXEL(&canvas->pixels[y + i][x + width - 1], vertical_pixel);
    }

    // Set corner characters
    canvas->pixels[y][x].symbol = borders[2];                          // Top-left
    canvas->pixels[y][x + width - 1].symbol = borders[3];              // Top-right
    canvas->pixels[y + height - 1][x].symbol = borders[4];             // Bottom-left
    canvas->pixels[y + height - 1][x + width - 1].symbol = borders[5]; // Bottom-right
}

/*
 * Add horizontasl separator line to screen
 * Used for visual separation of screen areas
 */
void tc_draw_hline(TermCanvas *canvas, int x, int y, const wchar_t *borders, Color foreground, Color background, TcEffect effect) {
    if (!canvas || !borders) return;
    if (y < 0 || x < 0) return;
    if (y >= canvas->height || x >= canvas->width) return;

    TcPixel pixel = (TcPixel) {background, foreground, borders[0], effect};
    for (int i = 0; i < canvas->width; ++i) {
        SET_PIXEL(&canvas->pixels[y][x + i], pixel);
    }
    // Set separator start/end characters
    canvas->pixels[y][x].symbol = borders[6];
    canvas->pixels[y][canvas->width - 1].symbol = borders[7];
}

/*
 * Add vertical separator line to screen
 * Used for visual separation of screen areas
 */
void tc_draw_vline(TermCanvas *canvas, int x, int y, const wchar_t *borders, Color foreground, Color background, TcEffect effect) {
    if (!canvas || !borders) return;
    if (y < 0 || x < 0) return;
    if (y >= canvas->height || x >= canvas->width) return;

    TcPixel pixel = (TcPixel) {background, foreground, borders[0], effect};
    for (int i = 0; i < canvas->height; ++i) {
        SET_PIXEL(&canvas->pixels[y + i][x], pixel);
    }
    // Set separator start/end characters
    canvas->pixels[y][x].symbol = borders[6];
    canvas->pixels[canvas->height - 1][x].symbol = borders[7];
}

/*
 * Print screen content to terminal
 * Outputs the entire screen buffer with formatting
 */
void tc_show(const TermCanvas *canvas) {
    if (!canvas) return;

    int buf_idx = 0;

    // Move cursor to the top-left corner (home position)
    buf_idx += swprintf(canvas->buffer + buf_idx, MAX_ANSI_LENGTH, L"\033[0;0H");

    // Initialize last colors/effect to a value that won't match any real pixel
    Color last_bg = COLOR_NONE;
    Color last_fg = COLOR_NONE;
    TcEffect last_effect = Effect_None;

    for (int y = 0; y < canvas->height; ++y) {
        for (int x = 0; x < canvas->width; ++x) {
            TcPixel px = canvas->pixels[y][x];

            // Buffer overflow check
            if ((int)buf_idx > canvas->buffer_size - MAX_ANSI_LENGTH) {
                canvas->buffer[buf_idx] = L'\0'; // Null-terminate the string
                wprintf(L"%ls", canvas->buffer); // Print the buffer
                buf_idx = 0;                     // Reset the index
            }

            // Check if colors or effect have changed
            if (px.background.color != last_bg.color ||
                px.foreground.color != last_fg.color ||
                px.effect != last_effect) {

                // Combine reset, effect, and color setting into one escape sequence
                buf_idx += swprintf(canvas->buffer + buf_idx, MAX_ANSI_LENGTH,
                                    L"\033[0m%s%s", // Reset and then set new attributes
                                    get_effect_ansi(px.effect),
                                    get_color_ansi(px.foreground, px.background, canvas->mode)
                                    );
                // Update last known colors/effect
                last_bg = px.background;
                last_fg = px.foreground;
                last_effect = px.effect;
            }

            // Add the character to the buffer
            canvas->buffer[buf_idx++] = px.symbol;
        }
        canvas->buffer[buf_idx++] = L'\n'; // Add a newline at the end of each row
    }

    canvas->buffer[buf_idx] = L'\0';        // Null-terminate the string
    wprintf(L"%ls\033[0m", canvas->buffer); // Print the entire buffer and reset
}


/*
 * Put pixel at specified position
 * Puts a pixel at the specified position with specified symbol, background and foreground formatting
 */
void tc_put_pixel(TermCanvas *canvas, int x, int y, wchar_t symbol, Color foreground, Color background, TcEffect effect) {
    if (!canvas) return; // NULL check
    if (x < 0 || y < 0) return;
    if (x >= canvas->width || y >= canvas->height) return;

    TcPixel pixel = (TcPixel) {background, foreground, symbol, effect}; // Create the pixel
    SET_PIXEL(&canvas->pixels[y][x], pixel); // Set the pixel using the macro
}


/*
 * Fill rectangular area with specified symbol
 * Fills the area with specified symbol, background and foreground formatting
 */
void tc_fill_area(TermCanvas *canvas, int x, int y, int width, int height, wchar_t symbol, Color foreground, Color background, TcEffect effect) {
    if (!canvas) return; // NULL check
    if (x < 0 || y < 0 || height <= 0 || width <= 0) return;
    if (x + width > canvas->width || y + height > canvas->height) return;

    TcPixel pixel = (TcPixel) {background, foreground, symbol, effect}; // Create the pixel
    for (int i = y; i < y + height; i++) {
        for (int j = x; j < x + width; j++) {
            SET_PIXEL(&canvas->pixels[i][j], pixel); // Set the pixel using the macro
        }
    }
}

/*
 * Insert text into screen
 * Inserts text into the screen at the specified position
 */
void tc_draw_text(TermCanvas *canvas, int x, int y, const char *text, Color foreground, Color background, TcEffect effect) {
    if (!canvas || !text) return; // Check for NULL pointers
    if(x < 0 || y < 0) return;
    if (y >= canvas->height || x >= canvas->width) return;

    int text_length;
    for (text_length = 0; text[text_length] != '\0'; text_length++);

    if (x + text_length > canvas->width) {
        text_length = canvas->width - x;  // Truncate if text goes beyond screen width
    }
    
    TcPixel pixel = (TcPixel) {background, foreground, ' ', effect};
    for (int i = 0; i < text_length; i++) {
        SET_PIXEL(&canvas->pixels[y][x + i], pixel);
        canvas->pixels[y][x + i].symbol = text[i];
        canvas->pixels[y][x + i].effect = effect;
    }
}

/*
 * Insert wide text into screen
 * Inserts wide text into the screen at the specified position
 */
void tc_draw_wtext(TermCanvas *canvas, int x, int y, const wchar_t *text, Color foreground, Color background, TcEffect effect) {
    if (!canvas || !text) return; // Check for NULL pointers
    if(x < 0 || y < 0) return;
    if (y >= canvas->height || x >= canvas->width) return;

    int text_length;
    for (text_length = 0; text[text_length] != L'\0'; text_length++);

    if (x + text_length > canvas->width) {
        text_length = canvas->width - x;  // Truncate if text goes beyond screen width
    }
    
    TcPixel pixel = (TcPixel) {background, foreground, ' ', effect};
    for (int i = 0; i < text_length; i++) {
        SET_PIXEL(&canvas->pixels[y][x + i], pixel);
        canvas->pixels[y][x + i].symbol = text[i];
        canvas->pixels[y][x + i].effect = effect;
    }
}
#endif // TERMCANVAS_IMPLEMENTATION

#endif // TERMCANVAS_H
