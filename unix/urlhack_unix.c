#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "urlhack.h"

void urlhack_launch_url(const char* app, const char *url)
{
	const size_t max = 4000;
	char buf[max];
	const char *browser = NULL;

	if (app)
		browser = app;
	if (NULL == browser || 0 == strlen(browser))
		browser = "xdg-open";

	snprintf(buf, max, "%s %s", browser, url);

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



