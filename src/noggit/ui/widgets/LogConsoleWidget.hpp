// This file is part of Noggit3, licensed under GNU General Public License (version 3).
#pragma once

#include <noggit/Log.h>

#include <QWidget>

#include <QStringList>

#include <functional>

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QTimer;

namespace Noggit::Ui
{
  // Fix for issue #50 (error console): dockable panel showing the output of
  // the logging macros (Log/LogDebug/LogError), with an errors-only toggle
  // and a text filter. The full session log is available, including the
  // messages logged before the widget was created.
  class LogConsoleWidget : public QWidget
  {
    Q_OBJECT

  public:
    explicit LogConsoleWidget(QWidget* parent = nullptr);
    ~LogConsoleWidget() override;

    // invoked (in the GUI thread) whenever an error line arrives
    std::function<void()> on_error;

  private:
    struct sink final : Noggit::LogConsole::console_sink
    {
      explicit sink(LogConsoleWidget* widget) : _widget(widget) {}
      void append_line(std::string const& line) override;
      LogConsoleWidget* _widget;
    };

    void append_line(QString const& line);
    void flush_pending_lines();
    void refill();
    bool passes_filter(QString const& line) const;

    QPlainTextEdit* _text = nullptr;
    QLineEdit* _filter = nullptr;
    QCheckBox* _errors_only = nullptr;
    QTimer* _flush_timer = nullptr;

    QStringList _all_lines;
    QStringList _pending_lines;
    sink _sink;
    bool _error_seen = false;

    static constexpr int max_stored_lines = 20000;
  };
}
