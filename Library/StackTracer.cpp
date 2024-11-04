#if defined(__linux__) || defined(__linux) || defined(linux)

#include <execinfo.h>
#include <csignal>
#include <unistd.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <array>
#include <memory>
#include <string>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <ctime>

namespace debugging
{

    class StackTracer
    {
    public:
        static StackTracer &instance()
        {
            static StackTracer inst;
            return inst;
        }

    private:
        static constexpr size_t MAX_STACK_FRAMES = 50;

        StackTracer() noexcept
        {
            setup();
        }

        ~StackTracer() = default;
        StackTracer(const StackTracer &) = delete;
        StackTracer &operator=(const StackTracer &) = delete;
        StackTracer(StackTracer &&) = delete;
        StackTracer &operator=(StackTracer &&) = delete;

        static std::string demangle(const char *symbol) noexcept
        {
            int status = 0;
            std::unique_ptr<char, void (*)(void *)> demangled(
                abi::__cxa_demangle(symbol, nullptr, nullptr, &status),
                std::free);

            return (status == 0 && demangled) ? std::string(demangled.get()) : std::string(symbol);
        }

        static void handleSignal(int sig) noexcept
        {
            std::array<void *, MAX_STACK_FRAMES> array;
            size_t size = backtrace(array.data(), array.size());

            std::stringstream ss;
            ss << "\nAIS-catcher - Internal error: signal " << sig;
            switch (sig)
            {
            case SIGSEGV:
                ss << " - Segmentation fault";
                break;
            case SIGABRT:
                ss << " - Abort";
                break;
            case SIGFPE:
                ss << " - Floating Point Exception";
                break;
            case SIGILL:
                ss << " - Illegal Instruction";
                break;
            case SIGBUS:
                ss << " - Bus Error";
                break;
            default:
                ss << " - Unknown signal";
                break;
            }
            ss << "\n\nStack trace:\n";

            std::unique_ptr<char *, void (*)(void *)> messages(
                backtrace_symbols(array.data(), size),
                std::free);

            if (messages)
            {
                for (size_t i = 1; i < size; ++i)
                {
                    ss << formatStackFrame(i, messages.get()[i], array[i]);
                }
            }

            std::cerr << ss.str() << std::flush;

            ::signal(sig, SIG_DFL);
            ::raise(sig);
        }

        void setup() noexcept
        {
            ::signal(SIGSEGV, handleSignal);
            ::signal(SIGABRT, handleSignal);
            ::signal(SIGFPE, handleSignal);
            ::signal(SIGILL, handleSignal);
            ::signal(SIGBUS, handleSignal);
        }

        static std::string formatStackFrame(size_t frameIndex, const char *symbol, void *addr) noexcept
        {
            std::string frame(symbol);
            std::stringstream ss;
            ss << "[" << frameIndex << "]: ";

            auto parenPos = frame.find('(');
            auto plusPos = frame.find('+', parenPos);
            auto endPos = frame.find(')', plusPos);

            if (parenPos != std::string::npos &&
                plusPos != std::string::npos &&
                endPos != std::string::npos)
            {

                std::string binary = frame.substr(0, parenPos);
                std::string mangledName = frame.substr(parenPos + 1, plusPos - parenPos - 1);
                std::string offset = frame.substr(plusPos + 1, endPos - plusPos - 1);

                Dl_info info;
                if (dladdr(addr, &info))
                {
                    ss << binary << ": "
                       << demangle(mangledName.c_str()) << " +"
                       << offset << " ("
                       << info.dli_fname << ")\n";
                }
                else
                {
                    ss << binary << ": "
                       << demangle(mangledName.c_str()) << " +"
                       << offset << "\n";
                }
            }
            else
            {
                ss << frame << "\n";
            }

            return ss.str();
        }
    };

    namespace
    {
        const StackTracer &stackTracerInstance = StackTracer::instance();
    }

}

#endif