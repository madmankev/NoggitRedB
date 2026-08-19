// This file is part of Noggit3, licensed under GNU General Public License (version 3).
#include <noggit/ui/widgets/AssetTestDialog.hpp>

#include <noggit/Log.h>
#include <noggit/ModelManager.h>
#include <noggit/WMO.h>
#include <noggit/application/NoggitApplication.hpp>
#include <noggit/project/CurrentProject.hpp>
#include <noggit/ui/tools/PreviewRenderer/PreviewRenderer.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <algorithm>
#include <exception>

namespace Noggit::Ui
{
  AssetTestDialog::AssetTestDialog(QWidget* parent)
    : QDialog(parent)
    , _m2_checkbox(new QCheckBox(tr("M2 models"), this))
    , _wmo_checkbox(new QCheckBox(tr("WMOs"), this))
    , _status_label(new QLabel(tr("Idle."), this))
    , _progress_bar(new QProgressBar(this))
    , _log_view(new QPlainTextEdit(this))
    , _start_button(new QPushButton(tr("Start test"), this))
    , _cancel_button(new QPushButton(tr("Cancel"), this))
  {
    // Issue #7
    setWindowTitle(tr("Asset render test (dev)"));
    resize(500, 500);

    auto layout = new QVBoxLayout(this);

    auto info_label = new QLabel(
      tr("Loads and offscreen renders every selected asset type from the client "
         "data files to find assets that fail or crash the editor.\n"
         "Each asset is written to the report file before it is rendered, so if "
         "the application crashes, the last line of the report pinpoints the "
         "offending asset."), this);
    info_label->setWordWrap(true);
    layout->addWidget(info_label);

    auto options_layout = new QHBoxLayout(this);
    _m2_checkbox->setChecked(true);
    _wmo_checkbox->setChecked(true);
    options_layout->addWidget(_m2_checkbox);
    options_layout->addWidget(_wmo_checkbox);
    options_layout->addStretch();
    layout->addLayout(options_layout);

    _progress_bar->setMinimum(0);
    _progress_bar->setValue(0);
    layout->addWidget(_progress_bar);

    layout->addWidget(_status_label);

    _log_view->setReadOnly(true);
    _log_view->setMaximumBlockCount(5000);
    layout->addWidget(_log_view);

    auto buttons_layout = new QHBoxLayout(this);
    buttons_layout->addStretch();
    buttons_layout->addWidget(_start_button);
    buttons_layout->addWidget(_cancel_button);
    layout->addLayout(buttons_layout);

    _cancel_button->setEnabled(false);

    connect(_start_button, &QPushButton::clicked, this, &AssetTestDialog::runTest);
    connect(_cancel_button, &QPushButton::clicked, [this]()
    {
      // do not hard kill the loop, an asset can be in the middle of loading,
      // the loop cooperatively stops between assets instead
      _cancel_requested = true;
    });
  }

  void AssetTestDialog::append_log_line(QString const& line)
  {
    _log_view->appendPlainText(line);
  }

  std::vector<std::string> AssetTestDialog::gatherAssets() const
  {
    // same filter as the asset browser: skip wmo group files (_000.wmo) and
    // lod files, only root WMOs are test rendered
    static const QRegularExpression wmo_group_and_lod_regex(".+_\\d{3}(_lod.+)*.wmo");

    std::vector<std::string> assets;

    auto const& path_map = Noggit::Application::NoggitApplication::instance()
      ->clientData()->listfile()->pathToFileDataIDMap();

    assets.reserve(path_map.size());

    for (auto const& key_pair : path_map)
    {
      QString const q_path = QString::fromStdString(key_pair.first).toLower();

      if (_m2_checkbox->isChecked() && q_path.endsWith(".m2"))
      {
        assets.push_back(key_pair.first);
        continue;
      }

      if (_wmo_checkbox->isChecked()
          && q_path.endsWith(".wmo")
          && !wmo_group_and_lod_regex.match(q_path).hasMatch())
      {
        assets.push_back(key_pair.first);
      }
    }

    return assets;
  }

  void AssetTestDialog::runTest()
  {
    if (_running)
    {
      return;
    }

    auto assets = gatherAssets();
    std::sort(assets.begin(), assets.end());

    if (assets.empty())
    {
      _status_label->setText(tr("No assets found for the selected filters."));
      return;
    }

    if (QMessageBox::question(this
        , tr("Asset render test")
        , tr("This will attempt to load and render %1 assets. It can take a very "
             "long time and may crash the editor on a broken asset (finding those "
             "is the point of this test).\n\nStart the test?").arg(assets.size())
        , QMessageBox::Yes | QMessageBox::No
        , QMessageBox::No) != QMessageBox::Yes)
    {
      return;
    }

    _running = true;
    _cancel_requested = false;
    _start_button->setEnabled(false);
    _cancel_button->setEnabled(true);
    _progress_bar->setMaximum(static_cast<int>(assets.size()));
    _progress_bar->setValue(0);

    // the report is stored with the project, every asset is logged before being
    // rendered and the file is flushed, so a crash points at the broken asset
    QString report_dir = QString(Noggit::Project::CurrentProject::get()->ProjectPath.c_str());
    if (!(report_dir.endsWith('\\') || report_dir.endsWith('/')))
    {
      report_dir += "/";
    }

    QString const report_path = report_dir + "asset_test_report.txt";
    QFile report_file(report_path);

    if (!report_file.open(QIODevice::WriteOnly | QFile::Truncate))
    {
      LogError << "Unable to write the asset test report to " << report_path.toStdString() << std::endl;
      QMessageBox::warning(this, tr("Asset render test"), tr("Could not write the report file:\n%1").arg(report_path));

      _running = false;
      _start_button->setEnabled(true);
      _cancel_button->setEnabled(false);
      return;
    }

    report_file.write("# noggit asset render test report\n");
    report_file.write("# lines starting with 'testing' are written before the asset is rendered,\n");
    report_file.write("# so the last one in the file is the asset the editor crashed on (if it did)\n\n");
    report_file.flush();

    Noggit::Ui::Tools::PreviewRenderer preview_renderer(128, 128
      , Noggit::NoggitRenderContext::OBJECT_PALETTE_PREVIEW, this);
    preview_renderer.setVisible(false);

    // initialize the offscreen rendering context the same way the object palette does
    try
    {
      preview_renderer.setModelOffscreen("world/wmo/azeroth/buildings/human_farm/farm.wmo");
      preview_renderer.renderToPixmap();
    }
    catch (...)
    {
      // a non enUS/WotLK style client can lack the init asset, the first real
      // asset of the test will initialize the context instead
    }

    std::size_t rendered_ok = 0;
    std::size_t failed = 0;

    for (std::size_t i = 0; i < assets.size(); ++i)
    {
      if (_cancel_requested)
      {
        report_file.write("\n# test cancelled by the user\n");
        break;
      }

      std::string const& asset = assets[i];

      _status_label->setText(tr("%1/%2 : %3").arg(i + 1).arg(assets.size()).arg(asset.c_str()));
      _progress_bar->setValue(static_cast<int>(i));

      report_file.write(("testing " + asset + "\n").c_str());
      report_file.flush();

      // keep the UI responsive between assets and bound the memory use of the
      // various caches, unloaded data is re-uploaded on demand by the renderers
      if ((i & 0x0FF) == 0)
      {
        qApp->processEvents();
      }

      if ((i & 0x7FF) == 0 && i != 0)
      {
        Log << "asset render test: " << i << " / " << assets.size() << " done" << std::endl;
        preview_renderer.clearPixmapCache();
        ModelManager::unload_all(Noggit::NoggitRenderContext::OBJECT_PALETTE_PREVIEW);
        WMOManager::unload_all(Noggit::NoggitRenderContext::OBJECT_PALETTE_PREVIEW);
      }

      try
      {
        preview_renderer.setModelOffscreen(asset);
        preview_renderer.renderToPixmap();
        ++rendered_ok;
      }
      catch (std::exception const& e)
      {
        ++failed;
        report_file.write(("FAIL (exception): " + asset + " : " + std::string(e.what()) + "\n").c_str());
        report_file.flush();
        LogError << "asset render test failed for " << asset << " : " << e.what() << std::endl;
        append_log_line(tr("FAIL: %1 : %2").arg(asset.c_str()).arg(e.what()));
      }
      catch (...)
      {
        ++failed;
        report_file.write(("FAIL (unhandled exception): " + asset + "\n").c_str());
        report_file.flush();
        LogError << "asset render test failed for " << asset << " (unhandled exception)" << std::endl;
        append_log_line(tr("FAIL: %1").arg(asset.c_str()));
      }
    }

    qApp->processEvents();
    _progress_bar->setValue(static_cast<int>(assets.size()));

    auto const summary = tr("Done: %1 assets rendered, %2 failed.\nReport written to %3")
      .arg(rendered_ok)
      .arg(failed)
      .arg(report_path);

    report_file.write(("\n# done: " + std::to_string(rendered_ok) + " rendered ok, "
      + std::to_string(failed) + " failed\n").c_str());
    report_file.close();

    _status_label->setText(summary);
    append_log_line(summary);
    Log << "asset render test done: " << rendered_ok << " rendered, " << failed << " failed" << std::endl;

    _running = false;
    _start_button->setEnabled(true);
    _cancel_button->setEnabled(false);
  }
}
