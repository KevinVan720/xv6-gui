#ifndef __ASSEMBLER__

#define MAX_WIDGET_SIZE 50
#define MAX_SHORT_STRLEN 50
#define MAX_LONG_STRLEN 500

#include "gui.h"
#include "window_manager.h"

#define KEY_HOME 0xE0
#define KEY_END 0xE1
#define KEY_UP 0xE2
#define KEY_DN 0xE3
#define KEY_LF 0xE4
#define KEY_RT 0xE5
#define KEY_PGUP 0xE6
#define KEY_PGDN 0xE7
#define KEY_INS 0xE8
#define KEY_DEL 0xE9

#define CHARACTER_WIDTH 9
#define CHARACTER_HEIGHT 18

struct Widget;
// @para: window widget_index message
typedef void(*Handler)(struct Widget * widget, struct message *msg);

// @para: window widget_index
//typedef void(*painter)(struct window *win);

#define TEXT 0
#define INPUTFIELD 1
#define BUTTON 2
#define ICON 3
#define COLORFILL 4

typedef struct ColorFill {
    struct RGB* buf;
} ColorFill;

typedef struct Button {
    struct RGBA color;
    struct RGBA bg_color;
    char text[MAX_SHORT_STRLEN];
} Button;

typedef struct Text {
    struct RGBA color;
    char text[MAX_LONG_STRLEN];
} Text;

typedef struct InputField {
    struct RGBA color;
    char text[MAX_LONG_STRLEN];
    int current_pos;
} InputField;

typedef struct Icon {
    struct RGBA color;
    struct RGBA bg_color;
    int icon;
} Icon;

typedef union widget_base {
    ColorFill* colorfill;
    Button *button;
    Text *text;
    InputField *inputfield;
    Icon * icon;
} widget_base;

typedef struct Widget {
    widget_base context;
    int type;
    win_rect position;
    int scrollable;
    int next, prev;
    Handler handler;
} Widget;

typedef struct window {
    win_rect initialPosition;
    int hasTitleBar;
    struct RGB *window_buf;
    int width;
    int height;
    int scrollOffset;
    int handler;
    struct Widget widgets[MAX_WIDGET_SIZE];
    int widgetlisthead, widgetlisttail;
    int keyfocus;
    int needsRepaint;
} window;

typedef window* window_p;

#endif