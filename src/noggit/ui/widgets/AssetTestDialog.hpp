// This file is part of Noggit3, licensed under GNU General Public License (version 3).
#pragma once

#include <QDialog>

#include <string>
#include <vector>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

namespace Noggit::Ui
{
  // Issue #7 (optional runtime test coverage for reading and rendering assets):
  // dev tool that loads and offscreen renders every M2/WMO asset found in the
  // client data files, to investigate potential crashes coming from assets.
  // Each asset is logged to the report file before it is rendered, so when the
  // application does crash, the report pinpoints the offending asset.
  class AssetTestDialog : public QDialog
  {
    Q_OBJECT

  public:
    explicit AssetTestDialog(QWidget* parent = nullptr);

  private:
    void runTest();
    std::vector<std::string> gatherAssets() const;
    void append_log_line(QString const& line);

    QCheckBox* _m2_checkbox;
    QCheckBox* _wmo_checkbox;
    QLabel* _status_label;
    QProgressBar* _progress_bar;
    QPlainTextEdit* _log_view;
    QPushButton* _start_button;
    QPushButton* _cancel_button;

    bool _running = false;
    bool _cancel_requested = false;
  };
}
