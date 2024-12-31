//
// Created by chengxiao on 11/05/23.
//

#ifndef PSTA_LOGGER_H
#define PSTA_LOGGER_H
#include <iostream>
#include <sstream>
#include <fstream>
namespace SVF{
/// \brief Log level, from least to most verbose
enum class LogLevel {
    None = 60,
    Critical = 50,
    Error = 40,
    Warning = 30,
    Info = 20,
    Debug = 10,
    All = 0
};

/// \brief Base class for loggers
class Logger {

protected:
    /// \brief Output stream
    std::ostream& _out;
    std::ofstream& _file;
    std::ofstream& _null;

public:
    /// \brief constructor
    explicit Logger(std::ostream& out, std::ofstream& file, std::ofstream& null) noexcept
            :_out(out), _file(file), _null(null) {
    }

    /// \brief No copy constructor
    Logger(const Logger&) = delete;

    /// \brief No move constructor
    Logger(Logger&&) = delete;

    /// \brief No copy assignment operator
    Logger& operator=(const Logger&) = delete;

    /// \brief No move assignment operator
    Logger& operator=(Logger&&) = delete;

    /// \brief Destructor
    virtual ~Logger() {
        _file.close();
    }

    std::ostream& stream() const {
        return _out;
    }

    std::ostream& file() const {
        return _file;
    }

    std::ostream& null() const {
        return _null;
    }

    static void releaseLogger() {
        delete DefaultLogger;
        DefaultLogger = nullptr;
    }

    static Logger* DefaultLogger;
    static LogLevel Level;
    static std::string TraceFilename;

}; // end class Logger

/// \brief Get the logger
inline Logger& get_logger() {
    if (!Logger::DefaultLogger) {
        static std::ofstream fnull("/dev/null");
        static std::ofstream flog;
        std::string filename = Logger::TraceFilename;
        // if TraceFilename is not set, will init a null stream
        if (filename.size() > 0) {
            flog.open(filename);
            Logger::DefaultLogger = new Logger(std::cout, flog, fnull);
        } else {
            Logger::DefaultLogger = new Logger(std::cout, fnull, fnull);
        }
    }
    return *Logger::DefaultLogger;
}


inline std::ostream& Log(const LogLevel& level)  {
    Logger& logger = get_logger();
    // if level is lower than setting, return null stream
    if (Logger::Level <= level)
        return logger.stream();
    else
        return logger.null();
}

inline std::ostream& Dump()  {
    if (Logger::TraceFilename.size() > 0)
        return get_logger().file();
    else
        return get_logger().null();
}

}
#endif //PSTA_LOGGER_H
