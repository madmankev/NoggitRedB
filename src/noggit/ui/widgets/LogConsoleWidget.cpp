// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#include <noggit/ui/widgets/LogConsoleWidget.hpp>

#include <QCheckBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace Noggit::Ui
{
  void LogConsoleWidget::sink::append_line(std::string const& line)
  {
    // can be called from any thread: queue the work into the GUI thread.
    // The context overload guarantees the lambda is dropped when the
    // widget dies before the event is processed.
    QMetaObject::invokeMethod(
        _widget
      , [_widget = this->_widget, text = QString::fromStdString(line)]()
        {
          _widget->append_line(text);
        }
      , Qt::QueuedConnection
    );
  }

  LogConsoleWidget::LogConsoleWidget(QWidget* parent)
    : QWidget(parent)
    , _sink(this)
  {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);

    auto* toolbar = new QHBoxLayout();

    _errors_only = new QCheckBox(tr("Errors only"), this);
    toolbar->addWidget(_errors_only);

    _filter = new QLineEdit(this);
    _filter->setPlaceholderText(tr("Filter…"));
    _filter->setClearButtonEnabled(true);
    toolbar->addWidget(_filter, 1);

    auto* clear_button = new QPushButton(tr("Clear"), this);
    toolbar->addWidget(clear_button);

    layout->addLayout(toolbar);

    _text = new QPlainTextEdit(this);
    _text->setReadOnly(true);
    _text->setMaximumBlockCount(max_stored_lines);

    QFont monospace_font(QStringLiteral("Monospace"));
    monospace_font.setStyleHint(QFont::TypeWriter);
    _text->setFont(monospace_font);

    layout->addWidget(_text);

    _flush_timer = new QTimer(this);
    _flush_timer->setSingleShot(true);
    _flush_timer->setInterval(50);
    connect(_flush_timer, &QTimer::timeout, this, &LogConsoleWidget::flush_pending_lines);

    connect(_errors_only, &QCheckBox::toggled, this, &LogConsoleWidget::refill);
    connect(_filter, &QLineEdit::textChanged, this, &LogConsoleWidget::refill);
    connect(clear_button, &QPushButton::clicked
      , [this]()
        {
          _all_lines.clear();
          _pending_lines.clear();
          _text->clear();
        }
    );

    // prefill with everything logged since the session start
    for (auto const& line : Noggit::LogConsole::console_history())
    {
      _all_lines.append(QString::fromStdString(line));
    }
    refill();

    Noggit::LogConsole::set_console_sink(&_sink);
  }

  LogConsoleWidget::~LogConsoleWidget()
  {
    Noggit::LogConsole::set_console_sink(nullptr);
  }

  bool LogConsoleWidget::passes_filter(QString const& line) const
  {
    if (_errors_only->isChecked() && !line.contains(QLatin1String("[Error]")))
    {
      return false;
    }

    QString const filter_text = _filter->text();
    if (!filter_text.isEmpty() && !line.contains(filter_text, Qt::CaseInsensitive))
    {
      return false;
    }

    return true;
  }

  void LogConsoleWidget::append_line(QString const& line)
  {
    if (_all_lines.size() < max_stored_lines)
    {
      _all_lines.append(line);
    }

    if (line.contains(QLatin1String("[Error]")))
    {
      if (!_error_seen)
      {
        _error_seen = true;

        if (on_error)
        {
          on_error();
        }
      }
    }

    if (passes_filter(line))
    {
      _pending_lines.append(line);

      // coalesce bursts of messages into a single widget update
      if (!_flush_timer->isActive())
      {
        _flush_timer->start();
      }
    }
  }

  void LogConsoleWidget::flush_pending_lines()
  {
    if (_pending_lines.isEmpty())
    {
      return;
    }

    QScrollBar* const scroll_bar = _text->verticalScrollBar();
    bool const at_bottom = scroll_bar->value() >= scroll_bar->maximum() - 4;

    _text->appendPlainText(_pending_lines.join('\n'));
    _pending_lines.clear();

    if (at_bottom)
    {
      scroll_bar->setValue(scroll_bar->maximum());
    }
  }

  void LogConsoleWidget::refill()
  {
    QStringList visible_lines;
    visible_lines.reserve(_all_lines.size());

    for (auto const& line : _all_lines)
    {
      if (passes_filter(line))
      {
        visible_lines.append(line);
      }
    }

    _text->setPlainText(visible_lines.join('\n'));

    QScrollBar* const scroll_bar = _text->verticalScrollBar();
    scroll_bar->setValue(scroll_bar->maximum());
  }
}
