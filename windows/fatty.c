#include "putty.h"

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show) {
    enum {
        AS_GEN_LEN = 8,
        AS_AGENT_LEN = 10,
    };

    while (*cmdline && isspace(*cmdline))
        ++cmdline;

    if (!strncmp(cmdline, "--as-gen", AS_GEN_LEN)) {
        return puttygen_main(inst, prev, cmdline+AS_GEN_LEN, show);
    }
    if (!strncmp(cmdline, "--as-agent", AS_AGENT_LEN)) {
        return pageant_main(inst, prev, cmdline+AS_AGENT_LEN, show);
    }
    return putty_main(inst, prev, cmdline, show);
}
