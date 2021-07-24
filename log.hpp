#ifndef SRD_LOG
#define SRD_LOG
#include <iostream>
#include <string>

#define SRD_LOG_ESC "\033[0;"
#define SRD_LOG_END "m"
#define SRD_LOG_RED SRD_LOG_ESC "31" SRD_LOG_END
#define SRD_LOG_BLUE SRD_LOG_ESC "34" SRD_LOG_END
#define SRD_LOG_GREEN SRD_LOG_ESC "32" SRD_LOG_END
#define SRD_LOG_YELLOW SRD_LOG_ESC "34" SRD_LOG_END
#define SRD_LOG_NONE SRD_LOG_ESC "0" SRD_LOG_END
#define SRD_LOG_BOLD "\033[1;0m"

namespace srd::log
{
    struct logger
    {
        std::ostream &stream = std::cout;
        std::string prefix = "";
        std::string postfix = "";
        int indent = 0;
        bool shouldDecorate = true;
    };

    template<typename T>
    concept Loggable = requires(std::ostream &s, T x) { s << x; };

    template<Loggable T>
    logger &operator<<(logger &log, const T &data)
    {
        if(log.shouldDecorate)
            log.stream << log.prefix << std::string(log.indent * 2, ' ');

        log.shouldDecorate = false;
        log.stream << data << std::flush;
        return log;
    }

    extern logger cout, cerr, cwrn, csec;
    struct {} call;
    struct {} endl;

    template<typename T>
    void sec   (T &log) { ++log.indent; };

    template<typename T>
    void endsec(T &log) { --log.indent; };

    void sec   (decltype(call)) { ++cout.indent; ++cwrn.indent; ++cerr.indent; ++cout.indent; }
    void endsec(decltype(call)) { --cout.indent; --cwrn.indent; --cerr.indent; --cout.indent; }


    logger &operator<<(logger &log, decltype(endl) _)
    {
        log.shouldDecorate = true;
        log.stream << log.postfix << std::endl;
        return log;
    }
}

#endif // SRD_LOG
