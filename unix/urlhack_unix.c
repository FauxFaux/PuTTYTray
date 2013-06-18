#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "urlhack.h"

enum { max = 4000 };
void urlhack_launch_url(const char* app, const wchar_t *url)
{
	char buf[max];
	const char *browser = NULL;

	if (app)
		browser = app;
	if (NULL == browser || 0 == strlen(browser))
		browser = "xdg-open";

	strncat(buf, browser, max - 1);
	strcat(buf, " ");
	wcstombs(buf + strlen(buf), url, max - strlen(buf) - 2);
	if (!system(buf))
		printf("couldn't run browser: %s", buf);
}

int urlhack_is_ctrl_pressed()
{
	// TODO
	return 1;
}


void rtfm(const char *error)
{
	// TODO
}



