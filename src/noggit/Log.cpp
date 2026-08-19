// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#include <noggit/Log.h>

#include <cstddef>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <mutex>
#include <streambuf>

std::ostream& _LogError(const char * pFile, int pLine)
{
  return std::cerr << clock() * 1000 / CLOCKS_PER_SEC << " - (" << ((strrchr(pFile, '/') ? strrchr(pFile, '/') : (strrchr(pFile, '\\') ? strrchr(pFile, '\\') : pFile - 1)) + 1) << ":" << pLine << "): [Error] ";
}
std::ostream& _LogDebug(const char * pFile, int pLine)
{
  return std::clog << clock() * 1000 / CLOCKS_PER_SEC << " - (" << ((strrchr(pFile, '/') ? strrchr(pFile, '/') : (strrchr(pFile, '\\') ? strrchr(pFile, '\\') : pFile - 1)) + 1) << ":" << pLine << "): [Debug] ";
}
std::ostream& _Log(const char * pFile, int pLine)
{
  return std::cout << clock() * 1000 / CLOCKS_PER_SEC << " - (" << ((strrchr(pFile, '/') ? strrchr(pFile, '/') : (strrchr(pFile, '\\') ? strrchr(pFile, '\\') : pFile - 1)) + 1) << ":" << pLine << "): ";
}

// Fix for issue #50: logging fan-out (console/file + in-editor console)
namespace
{
  std::mutex g_console_log_mutex;
  Noggit::LogConsole::console_sink* g_console_sink = nullptr;
  std::deque<std::string> g_console_history;
  std::string g_pending_line;

  constexpr std::size_t max_history_lines = 5000;
  constexpr std::size_t max_line_length = 4096;

  // requires g_console_log_mutex to be held
  void sink_line(std::string const& line)
  {
    g_console_history.emplace_back(line);
    while (g_console_history.size() > max_history_lines)
    {
      g_console_history.pop_front();
    }

    if (g_console_sink)
    {
      g_console_sink->append_line(line);
    }
  }
}

void Noggit::LogConsole::set_console_sink(console_sink* sink)
{
  std::lock_guard<std::mutex> const lock (g_console_log_mutex);
  g_console_sink = sink;
}

Noggit::LogConsole::console_sink* Noggit::LogConsole::get_console_sink()
{
  std::lock_guard<std::mutex> const lock (g_console_log_mutex);
  return g_console_sink;
}

std::vector<std::string> Noggit::LogConsole::console_history()
{
  std::lock_guard<std::mutex> const lock (g_console_log_mutex);
  return {g_console_history.begin(), g_console_history.end()};
}

void Noggit::LogConsole::console_log_write(const char* data, std::size_t size)
{
  std::lock_guard<std::mutex> const lock (g_console_log_mutex);

  for (std::size_t i (0); i < size; ++i)
  {
    char const c (data[i]);

    if (c == '\n')
    {
      sink_line(g_pending_line);
      g_pending_line.clear();
    }
    else if (c != '\r')
    {
      g_pending_line += c;

      // forward overlong lines in pieces instead of buffering unbounded
      if (g_pending_line.size() > max_line_length)
      {
        sink_line(g_pending_line);
        g_pending_line.clear();
      }
    }
  }
}

namespace
{
  // writes to the original target (console stream or log.txt) and feeds the
  // in-editor log console at the same time
  class tee_streambuf : public std::streambuf
  {
  public:
    explicit tee_streambuf(std::streambuf* primary)
      : _primary(primary)
    {}

  protected:
    std::streamsize xsputn(const char* s, std::streamsize n) override
    {
      if (_primary)
      {
        _primary->sputn(s, n);
      }
      Noggit::LogConsole::console_log_write(s, static_cast<std::size_t>(n));
      return n;
    }

    int overflow(int c) override
    {
      if (!traits_type::eq_int_type(c, traits_type::eof()))
      {
        char const ch = traits_type::to_char_type(c);

        if (_primary)
        {
          _primary->sputc(ch);
        }
        Noggit::LogConsole::console_log_write(&ch, 1);
      }
      return traits_type::not_eof(c);
    }

    int sync() override
    {
      return _primary ? _primary->pubsync() : 0;
    }

  private:
    std::streambuf* _primary;
  };

  std::ofstream gLogStream;
}

#if DEBUG__LOGGINGTOCONSOLE
void InitLogging()
{
  static tee_streambuf cout_tee (std::cout.rdbuf());
  static tee_streambuf clog_tee (std::clog.rdbuf());
  static tee_streambuf cerr_tee (std::cerr.rdbuf());
  std::cout.rdbuf(&cout_tee);
  std::clog.rdbuf(&clog_tee);
  std::cerr.rdbuf(&cerr_tee);

  LogDebug << "Logging to console window." << std::endl;
}
#else
void InitLogging()
{
  // Set up log.
  gLogStream.open("log.txt", std::ios_base::out | std::ios_base::trunc);
  if (gLogStream)
  {
    static tee_streambuf cout_tee (gLogStream.rdbuf());
    static tee_streambuf clog_tee (gLogStream.rdbuf());
    static tee_streambuf cerr_tee (gLogStream.rdbuf());
    std::cout.rdbuf(&cout_tee);
    std::clog.rdbuf(&clog_tee);
    std::cerr.rdbuf(&cerr_tee);
  }
  else
  {
    // even without a writable log file the messages are still kept for the
    // in-editor log console (and still reach the real console)
    static tee_streambuf cout_tee (std::cout.rdbuf());
    static tee_streambuf clog_tee (std::clog.rdbuf());
    static tee_streambuf cerr_tee (std::cerr.rdbuf());
    std::cout.rdbuf(&cout_tee);
    std::clog.rdbuf(&clog_tee);
    std::cerr.rdbuf(&cerr_tee);
  }
}
#endif
