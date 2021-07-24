// srd::core's own implementation.
#include "log.hpp"

namespace srd::log
{
    logger cout { .prefix = SRD_LOG_BLUE   "➜ ", .postfix = SRD_LOG_NONE };
    logger cerr { .prefix = SRD_LOG_RED    "✘ ", .postfix = SRD_LOG_NONE };
    logger cwrn { .prefix = SRD_LOG_YELLOW "⚠ ", .postfix = SRD_LOG_NONE };
    logger csec { .prefix = SRD_LOG_BOLD "⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯ ", .postfix = " ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯" SRD_LOG_NONE };
}
