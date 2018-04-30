#include "regex_lib.h"

#include <regex>
#include <assert.h>

static void (*rtfm_func)(const char *error);

struct regexp {
    std::regex *inner;
};

struct regexp *regcomp(const char *expr)
{
  try {
    auto reg = new std::regex(
        expr, std::regex_constants::ECMAScript | std::regex_constants::icase);
    regexp *ret = (regexp *)calloc(1, sizeof(regexp));
    ret->inner = reg;
    return ret;
  } catch (std::regex_error &error) {
    rtfm_func(error.what());
  }
}

int regexec(struct regexp *regex, char *text)
{
  assert(!"not implemented");
}

void set_regerror_func(void (*rtfm)(const char *error))
{
  rtfm_func = rtfm;
}

void reg_get_range(struct regexp *regex, char **start, char **end)
{
  assert(!"not implemented");
}
