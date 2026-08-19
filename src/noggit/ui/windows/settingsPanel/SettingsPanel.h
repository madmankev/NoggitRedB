// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#pragma once

#include <QMainWindow>

namespace Ui
{
  class SettingsPanel;
}

class QCheckBox;
class QSettings;

namespace Noggit
{
  namespace Ui
  {
    class settings : public QMainWindow
    {
      Q_OBJECT
      QSettings* _settings;
      ::Ui::SettingsPanel* ui;
      // Fix for issue #63: created programmatically (not part of the .ui file)
      QCheckBox* _shader_hot_reload_cb = nullptr;
    public:
      settings(QWidget* parent = nullptr);
      void discard_changes();
      void save_changes();

    signals:
      void saved();
    };
  }
}
