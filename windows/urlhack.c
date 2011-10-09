#include <windows.h>
#include <string.h>
#include "urlhack.h"
#include "misc.h"
#include "puttymem.h"


extern int urlhack_mouse_old_x = -1, urlhack_mouse_old_y = -1, urlhack_current_region = -1;

static text_region **link_regions;
static unsigned int link_regions_len;
static unsigned int link_regions_current_pos;

void urlhack_launch_url(const char* app, const char *url);
int urlhack_is_ctrl_pressed();
void rtfm(char *error);

int urlhack_is_in_link_region(int x, int y)
{
    unsigned int i = 0;

    while (i != link_regions_current_pos) {
        text_region r = *link_regions[i];

        if ((r.y0 == r.y1 && y == r.y0 && y == r.y1 && x >= r.x0 && x < r.x1) ||
            (r.y0 != r.y1 && ((y == r.y0 && x >= r.x0) || (y == r.y1 && x < r.x1) || (y > r.y0 && y < r.y1))))
            return i+1;
        i++;
    }
    
    return 0;
}

int urlhack_is_in_this_link_region(text_region r, int x, int y)
{
    if ((r.y0 == r.y1 && y == r.y0 && y == r.y1 && x >= r.x0 && x < r.x1) || 
        (r.y0 != r.y1 && ((y == r.y0 && x >= r.x0) || (y == r.y1 && x < r.x1) || (y > r.y0 && y < r.y1)))) {
        return 1;
    }
    
    return 0;
}

text_region urlhack_get_link_bounds(int x, int y)
{
    unsigned int i = 0;
    text_region region;

    while (i != link_regions_current_pos) {
        text_region r = *link_regions[i];

        if ((r.y0 == r.y1 && y == r.y0 && y == r.y1 && x >= r.x0 && x < r.x1) ||
            (r.y0 != r.y1 && ((y == r.y0 && x >= r.x0) || (y == r.y1 && x < r.x1) || (y > r.y0 && y < r.y1)))) {
            return *link_regions[i];
        }

        i++;
    }

    region.x0 = region.y0 = region.x1 = region.y1 = -1;
    return region;
}

text_region urlhack_get_link_region(int index)
{
    text_region region;

    if (index < 0 || index >= link_regions_current_pos) {
        region.x0 = region.y0 = region.x1 = region.y1 = -1;
        return region;
    }
    else {
        return *link_regions[index];
    }
}

void urlhack_add_link_region(int x0, int y0, int x1, int y1)
{
    if (link_regions_current_pos >= link_regions_len) {
        unsigned int i;
        link_regions_len *= 2;
        link_regions = sresize(link_regions, link_regions_len, text_region*);
        for (i = link_regions_current_pos; i < link_regions_len; ++i) {
            link_regions[i] = NULL;
        }
    }

    link_regions[link_regions_current_pos] = snew(text_region);
    link_regions[link_regions_current_pos]->x0 = x0;
    link_regions[link_regions_current_pos]->y0 = y0;
    link_regions[link_regions_current_pos]->x1 = x1;
    link_regions[link_regions_current_pos]->y1 = y1;

    link_regions_current_pos++;
}

void urlhack_link_regions_clear()
{
    unsigned int i;
    for (i = 0; i < link_regions_len; ++i) {
        if (link_regions[i] != NULL) {
            sfree(link_regions[i]);
            link_regions[i] = NULL;
        }
    }
    link_regions_current_pos = 0;
}

// Regular expression stuff

static int urlhack_disabled = 0;
static int is_regexp_compiled = 0;
static regexp* urlhack_rx;

static char *window_text;
static int window_text_len;
static int window_text_current_pos;

void urlhack_init()
{
    unsigned int i;

    /* 32 links seems like a sane base value */
    link_regions_current_pos = 0;
    link_regions_len = 32;
    link_regions = snewn(link_regions_len, text_region*);

    for (i = 0; i < link_regions_len; ++i) {
        link_regions[i] = NULL;
    }

    /* Start with default terminal size */
    window_text_len = 80*24+1;
    window_text = snewn(window_text_len, char);
    urlhack_reset();
}

void urlhack_cleanup()
{
    urlhack_link_regions_clear();
    sfree(link_regions);
    sfree(window_text);
}

void urlhack_putchar(char ch)
{
    if (window_text_current_pos >= window_text_len) {
        window_text = sresize(window_text, 2 * window_text_len, char);
        memset(window_text + window_text_current_pos, '\0', window_text_len - window_text_current_pos);
        window_text_len *= 2;
    }
    window_text[window_text_current_pos++] = ch;
}

void urlhack_reset()
{
    memset(window_text, '\0', window_text_len);
    window_text_current_pos = 0;
}

void urlhack_set_regular_expression(const char* expression)
{
    is_regexp_compiled = 0;
    urlhack_disabled = 0;

    set_regerror_func(rtfm);
    urlhack_rx = regcomp((char*)(expression));

    if (urlhack_rx == 0) {
        urlhack_disabled = 1;
    }

    is_regexp_compiled = 1;
}

void urlhack_go_find_me_some_hyperlinks(int screen_width)
{
    char* text_pos;

    if (urlhack_disabled != 0) return;

    if (is_regexp_compiled == 0) {
        urlhack_set_regular_expression(urlhack_default_regex);
    }

    urlhack_link_regions_clear();

    text_pos = window_text;

    while (regexec(urlhack_rx, text_pos) == 1) {
        char* start_pos = *urlhack_rx->startp[0] == ' ' ? urlhack_rx->startp[0] + 1: urlhack_rx->startp[0];

        char* end_pos = urlhack_rx->endp[0];
        int max_brackets = 0, x0, y0, x1, y1;
        char *c;
        for (c = start_pos; c < end_pos; ++c) {
            switch (*c) {
                case '(': ++max_brackets; break;
                case ')': --max_brackets; break;
            }
        }
        while (max_brackets --> 0 && *end_pos == ')')
            ++end_pos;

        x0 = (start_pos - window_text) % screen_width;
        y0 = (start_pos - window_text) / screen_width;
        x1 = (end_pos - window_text) % screen_width;
        y1 = (end_pos - window_text) / screen_width;

        if (x0 >= screen_width) x0 = screen_width - 1;
        if (x1 >= screen_width) x1 = screen_width - 1;

        urlhack_add_link_region(x0, y0, x1, y1);

        text_pos = end_pos + 1;
    }
}
