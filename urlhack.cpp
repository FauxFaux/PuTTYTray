#include <vector>
#include <string>
#include <sstream>
#include "urlhack.h"

int urlhack_mouse_old_x = -1, urlhack_mouse_old_y = -1, urlhack_current_region = -1;

static std::vector<text_region> link_regions;

void urlhack_clear_link_regions()
{
	link_regions.clear();
}



int urlhack_is_in_link_region(int x, int y)
{
	std::vector<text_region>::iterator i = link_regions.begin();
	int loopCounter = 1;

	while (i != link_regions.end()) {
		text_region r = *i;

		if ((r.y0 == r.y1 && y == r.y0 && y == r.y1 && x >= r.x0 && x < r.x1) ||
			(r.y0 != r.y1 && ((y == r.y0 && x >= r.x0) || (y == r.y1 && x < r.x1) || (y > r.y0 && y < r.y1))))
			return loopCounter; //Changed: was return true

		loopCounter++;
		i++;
	}
	
	return false;
}



int urlhack_is_in_this_link_region(text_region r, int x, int y)
{
  	if ((r.y0 == r.y1 && y == r.y0 && y == r.y1 && x >= r.x0 && x < r.x1) || (r.y0 != r.y1 && ((y == r.y0 && x >= r.x0) || (y == r.y1 && x < r.x1) || (y > r.y0 && y < r.y1)))) {
		return true;
	}
	
	return false;
}



text_region urlhack_get_link_bounds(int x, int y)
{
	std::vector<text_region>::iterator i = link_regions.begin();

	while (i != link_regions.end()) {
		text_region r = *i;

		if ((r.y0 == r.y1 && y == r.y0 && y == r.y1 && x >= r.x0 && x < r.x1) ||
			(r.y0 != r.y1 && ((y == r.y0 && x >= r.x0) || (y == r.y1 && x < r.x1) || (y > r.y0 && y < r.y1))))
			return *i;

		i++;
	}

	text_region region;
	region.x0 = region.y0 = region.x1 = region.y1 = -1;
	return region;
}



text_region urlhack_get_link_region(int index)
{
	text_region region;

	if (index < 0 || index >= (int)link_regions.size()) {
		region.x0 = region.y0 = region.x1 = region.y1 = -1;
		return region;
	}
	else {
		return link_regions.at(index);
	}
}



void urlhack_add_link_region(int x0, int y0, int x1, int y1)
{
	text_region region;

	region.x0 = x0;
	region.y0 = y0;
	region.x1 = x1;
	region.y1 = y1;

	link_regions.insert(link_regions.end(), region);
}


// Regular expression stuff

static int urlhack_disabled = 0;
static int is_regexp_compiled = 0;
static regexp* urlhack_rx;
static std::string text_mass;



void urlhack_reset()
{
	text_mass.clear();
}

static void urlhack_putchar(char ch)
{
	char r00fles[2] = { ch, 0 };
	text_mass.append(r00fles);
}

void urlhack_putchar(char ch, int width)
{
    while (width --> 1)
        urlhack_putchar(' ');
    urlhack_putchar(ch);
}

void urlhack_set_regular_expression(const char* expression)
{
	is_regexp_compiled = 0;
	urlhack_disabled = 0;

	set_regerror_func(rtfm);
	urlhack_rx = regcomp(const_cast<char*>(expression));

	if (urlhack_rx == 0) {
		urlhack_disabled = 1;
	}

	is_regexp_compiled = 1;
}



void urlhack_go_find_me_some_hyperlinks(int screen_width)
{
	if (urlhack_disabled != 0) return;

	if (is_regexp_compiled == 0) {
		urlhack_set_regular_expression(urlhack_default_regex);
	}

	urlhack_clear_link_regions();


	char* text = const_cast<char*>(text_mass.c_str());
	char* text_pos = text;

	while (regexec(urlhack_rx, text_pos) == 1) {
		char* start_pos = *urlhack_rx->startp[0] == ' ' ? urlhack_rx->startp[0] + 1: urlhack_rx->startp[0];
		char* end_pos = urlhack_rx->endp[0];
		int max_brackets = 0;
		for (char *c = start_pos; c < end_pos; ++c) {
			switch (*c) {
				case '(': ++max_brackets; break;
				case ')': --max_brackets; break;
			}
		}
		while (max_brackets --> 0 && *end_pos == ')')
			++end_pos;

		int x0 = (start_pos - text) % screen_width;
		int y0 = (start_pos - text) / screen_width;
		int x1 = (end_pos - text) % screen_width;
		int y1 = (end_pos - text) / screen_width;

		if (x0 >= screen_width) x0 = screen_width - 1;
		if (x1 >= screen_width) x1 = screen_width - 1;

		urlhack_add_link_region(x0, y0, x1, y1);

		text_pos = end_pos + 1;
	}
}
