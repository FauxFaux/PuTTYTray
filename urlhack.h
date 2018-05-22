#pragma once
#ifndef _URLHACK_H
#define _URLHACK_H

#ifdef __cplusplus
extern "C" {
#endif
#include <wchar.h>

typedef struct {
  int x0, y0, x1, y1;
} text_region;

extern const char *urlhack_default_regex;
extern const char *urlhack_liberal_regex;
extern int urlhack_mouse_old_x, urlhack_mouse_old_y, urlhack_current_region;

void urlhack_reset();
void urlhack_go_find_me_some_hyperlinks(int screen_width);
void urlhack_putchar(char ch);
text_region urlhack_get_link_region(unsigned int index);

int urlhack_is_in_link_region(int x, int y);
int urlhack_is_in_this_link_region(text_region r, int x, int y);
text_region urlhack_get_link_bounds(int x, int y);
void urlhack_add_link_region(int x0, int y0, int x1, int y1);
void urlhack_launch_url(const char *app, const wchar_t *url);
int urlhack_is_ctrl_pressed();
void urlhack_set_regular_expression(int mode, const char *expression);
void rtfm(const char *error);

void urlhack_init();
void urlhack_cleanup();

#ifdef __cplusplus
}
#endif

#endif // _URLHACK_H
