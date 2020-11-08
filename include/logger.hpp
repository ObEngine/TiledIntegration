#include <spdlog/logger.h>

namespace obe::tiled_integration
{
    extern std::shared_ptr<spdlog::logger> logger;
    void init_logger();
}
