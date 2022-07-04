#include <logger.hpp>

#include <spdlog/spdlog.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#if defined(_WIN32) || defined(_WIN64)
#include <spdlog/sinks/wincolor_sink.h>
#elif defined(__ANDROID__)
#include <spdlog/sinks/android_sink.h>
#else
#include <spdlog/sinks/ansicolor_sink.h>
#endif


std::shared_ptr<spdlog::logger> logger;

void init_logger()
{
    auto dist_sink = std::make_shared<spdlog::sinks::dist_sink_st>();

    const auto sink1 = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
    const auto sink2
        = std::make_shared<spdlog::sinks::basic_file_sink_st>("debug.log");

    dist_sink->add_sink(sink1);
    dist_sink->add_sink(sink2);
    logger = std::make_shared<spdlog::logger>("Log", dist_sink);
    logger->set_pattern("[%H:%M:%S.%e]<%^%l%$> : %v");
    logger->set_level(spdlog::level::debug);
    logger->flush_on(spdlog::level::info);
    logger->info("Logger initialized");
}

