// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#pragma once

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

std::ostream& _LogError(const char * pFile, int pLine);
std::ostream& _LogDebug(const char * pFile, int pLine);
std::ostream& _Log(const char * pFile, int pLine);

#define LogError _LogError( __FILE__, __LINE__ )
#define LogDebug _LogDebug( __FILE__, __LINE__ )
#define Log _Log( __FILE__, __LINE__ )

void InitLogging();

// Fix for issue #50 (error console panel): everything written through the
// logging macros is also kept in a small rolling history and forwarded to
// an optional UI sink (see ui/widgets/LogConsoleWidget) so errors can be
// reviewed from within the editor.
namespace Noggit::LogConsole
{
  struct console_sink
  {
    virtual ~console_sink() = default;
    // called from arbitrary threads, one line at a time, without the
    // trailing line feed
    virtual void append_line(std::string const& line) = 0;
  };

  // only one sink at a time; the sink must be detached by passing nullptr
  // before its owner is destroyed
  void set_console_sink(console_sink* sink);
  console_sink* get_console_sink();

  // session history, oldest messages first, without trailing line feeds
  std::vector<std::string> console_history();

  // internal: used by the tee streambufs in Log.cpp
  void console_log_write(const char* data, std::size_t size);
}
