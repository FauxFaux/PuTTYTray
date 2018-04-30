#ifdef __cplusplus
extern "C" {
#endif

  struct regexp;
  struct regexp *regcomp(const char *expr);
  int regexec(struct regexp *regex, char *text);
  void set_regerror_func(void (*rtfm)(const char *error));
  void reg_get_range(struct regexp *regex, char **start, char **end);

#ifdef __cplusplus
}
#endif
