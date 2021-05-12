#include <memory>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>


namespace sdf
{

/* Ensures that logger "console" exists. */
class LoggerInitializer
{
    const std::shared_ptr<spdlog::logger> logger = spdlog::stdout_logger_mt("console", true);
public:
    LoggerInitializer()
    {
        logger->set_pattern("%H:%M:%S %v ");
    }
};

static const LoggerInitializer loggerInitializer; // NOLINT(cert-err58-cpp)

}