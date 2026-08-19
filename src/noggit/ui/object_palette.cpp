// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#include "object_palette.hpp"

#include <noggit/Log.h>
#include <noggit/MapView.h>
#include <noggit/project/ApplicationProject.h>
#include <noggit/ui/FontAwesome.hpp>
#include <noggit/ui/tools/AssetBrowser/Ui/AssetBrowser.hpp>
#include <noggit/ui/tools/PreviewRenderer/PreviewRenderer.hpp>
#include <noggit/World.h>

#include <QDockWidget>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QSignalBlocker>
#include <QtGui/QDrag>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include <string>
#include <unordered_set>


namespace Noggit
{
  namespace Ui
  {

    ObjectList::ObjectList(QWidget* parent) : QListWidget(parent)
    {
      setIconSize(QSize(100, 100));
      setViewMode(QListWidget::IconMode);
      setFlow(QListWidget::LeftToRight);
      setWrapping(true);
      setSelectionMode(QAbstractItemView::SingleSelection);
      setSelectionBehavior(QAbstractItemView::SelectItems);
      setAcceptDrops(false);
      setMovement(Movement::Static);
      setResizeMode(QListView::Adjust);
    }

    void ObjectList::mousePressEvent(QMouseEvent* event)
    {
      if (event->button() == Qt::LeftButton)
        _start_pos = event->pos();

      QListWidget::mousePressEvent(event);
    }

    void ObjectList::mouseMoveEvent(QMouseEvent* event)
    {
      QListWidget::mouseMoveEvent(event);

      if (!(event->buttons() & Qt::LeftButton))
        return;
      if ((event->pos() - _start_pos).manhattanLength()
          < QApplication::startDragDistance())
        return;

      const QList<QListWidgetItem*> selected_items = selectedItems();

      for (auto item: selected_items)
      {
        QMimeData* mimeData = new QMimeData;
        mimeData->setText(item->toolTip());


        QDrag* drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->setPixmap(item->icon().pixmap(100, 100));
        drag->exec();
        return;   // we assume only one item can be selected
      }

    }

    ObjectPalette::ObjectPalette(MapView* map_view, std::shared_ptr<Noggit::Project::NoggitProject> Project, QWidget* parent)
        : widget(parent), layout(new ::QGridLayout(this)), _map_view(map_view), _project(Project)
    {
      setWindowTitle("Object Palette");
      setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
      setMinimumSize(330, 100);
      setAcceptDrops(true);

      _object_paths = std::unordered_set<std::string>();
      _object_list = new ObjectList(this);

      layout->addWidget(_object_list, 0, 0);

      _preview_renderer = new Noggit::Ui::Tools::PreviewRenderer(_object_list->iconSize().width(),
                                                                 _object_list->iconSize().height(),
                                                                 Noggit::NoggitRenderContext::OBJECT_PALETTE_PREVIEW,
                                                                 this);
      _preview_renderer->setVisible(false);

      // just to initialize context, ugly-ish
      _preview_renderer->setModelOffscreen("world/wmo/azeroth/buildings/human_farm/farm.wmo");
      _preview_renderer->renderToPixmap();

      QObject::connect(_object_list, &QListWidget::itemSelectionChanged, [this]()
        {
          QListWidgetItem* const item = _object_list->currentItem();
          if (item)
          {
            emit selected(item->toolTip().toStdString());
          }
        }
      );

      QVBoxLayout* button_layout = new QVBoxLayout(this);

      _add_button = new QPushButton(this);
      _add_button->setToolTip("Add from Asset Browser");
      _add_button->setIcon(FontAwesomeIcon(FontAwesome::plus));
      button_layout->addWidget(_add_button);
      connect(_add_button, &QAbstractButton::clicked, this, &ObjectPalette::addObjectFromAssetBrowser);

      _remove_button = new QPushButton(this);
      _remove_button->setToolTip("Remove selected Object");
      _remove_button->setIcon(FontAwesomeIcon(FontAwesome::times));
      button_layout->addWidget(_remove_button);
      connect(_remove_button, &QAbstractButton::clicked, this, &ObjectPalette::removeSelectedTexture);

      button_layout->addStretch();

      layout->addLayout(button_layout, 0, 1);

      // Fix for issue #36: named palette controls. Palettes can be saved under a name,
      // reloaded on any map, deleted, and exported/imported as files to share with
      // other designers or move between workstations.
      auto palette_controls = new QHBoxLayout(this);

      _palette_selector = new QComboBox(this);
      _palette_selector->setToolTip("Saved palettes: select one to load it here");
      palette_controls->addWidget(_palette_selector, 1);
      connect(_palette_selector, QOverload<int>::of(&QComboBox::activated)
        , this, &ObjectPalette::loadNamedPalette);

      _save_named_button = new QPushButton(this);
      _save_named_button->setToolTip("Save current objects as a named palette");
      _save_named_button->setIcon(FontAwesomeIcon(FontAwesome::save));
      palette_controls->addWidget(_save_named_button);
      connect(_save_named_button, &QAbstractButton::clicked
        , this, &ObjectPalette::saveCurrentPaletteAsNamed);

      _delete_named_button = new QPushButton(this);
      _delete_named_button->setToolTip("Delete the selected named palette");
      _delete_named_button->setIcon(FontAwesomeIcon(FontAwesome::trash));
      palette_controls->addWidget(_delete_named_button);
      connect(_delete_named_button, &QAbstractButton::clicked
        , this, &ObjectPalette::deleteSelectedNamedPalette);

      _export_button = new QPushButton(this);
      _export_button->setToolTip("Export current objects to a palette file (to share it)");
      _export_button->setIcon(FontAwesomeIcon(FontAwesome::upload));
      palette_controls->addWidget(_export_button);
      connect(_export_button, &QAbstractButton::clicked
        , this, &ObjectPalette::exportPaletteToFile);

      _import_button = new QPushButton(this);
      _import_button->setToolTip("Import a palette file (from another designer/workstation)");
      _import_button->setIcon(FontAwesomeIcon(FontAwesome::download));
      palette_controls->addWidget(_import_button);
      connect(_import_button, &QAbstractButton::clicked
        , this, &ObjectPalette::importPaletteFromFile);

      layout->addLayout(palette_controls, 1, 0, 1, 2);

      refreshPaletteSelector();

      LoadSavedPalette();
    }


    void ObjectPalette::LoadSavedPalette()
    {
      unsigned int map_id = _map_view->getWorld()->getMapID();
      for (auto const& palette : _project->ObjectPalettes)
      {
        if (palette.MapId == map_id)
        {
          for (auto const& filename : palette.Filepaths)
              addObjectByFilename(filename.c_str(), false);
          break;
        }

      }
    }

    void ObjectPalette::SavePalette()
    {
        auto palette_obj = Noggit::Project::NoggitProjectObjectPalette();
        palette_obj.MapId = _map_view->getWorld()->getMapID();
        for (auto& path : _object_paths)
            palette_obj.Filepaths.push_back(path);

        _project->saveObjectPalette(palette_obj);
    }

    // Fix for issue #36: named palettes

    void ObjectPalette::refreshPaletteSelector(QString const& select_name)
    {
      const QSignalBlocker blocker(_palette_selector);

      _palette_selector->clear();

      // empty user data = the per-map auto saved palette
      _palette_selector->addItem("Map palette (per map)", QString());

      for (auto const& named_palette : _project->NamedObjectPalettes)
      {
        _palette_selector->addItem(named_palette.Name.c_str(), named_palette.Name.c_str());
      }

      int restore = _palette_selector->findData(select_name);
      _palette_selector->setCurrentIndex(restore >= 0 ? restore : 0);
    }

    void ObjectPalette::clearPaletteObjects()
    {
      _object_paths.clear();
      _object_list->clear();
    }

    void ObjectPalette::loadNamedPalette(int index)
    {
      if (index < 0)
        return;

      QString const name = _palette_selector->itemData(index).toString();

      clearPaletteObjects();

      if (name.isEmpty())
      {
        // the per-map auto saved palette
        LoadSavedPalette();
        return;
      }

      for (auto const& named_palette : _project->NamedObjectPalettes)
      {
        if (named_palette.Name == name.toStdString())
        {
          for (auto const& filename : named_palette.Filepaths)
          {
            addObjectByFilename(filename.c_str(), false);
          }
          return;
        }
      }
    }

    void ObjectPalette::saveCurrentPaletteAsNamed()
    {
      QString const current_name = _palette_selector->currentData().toString();

      bool ok = false;
      QString const name = QInputDialog::getText(this
        , tr("Save palette")
        , tr("Palette name:")
        , QLineEdit::Normal
        , current_name
        , &ok).trimmed();

      if (!ok || name.isEmpty())
        return;

      auto palette_obj = Noggit::Project::NoggitProjectObjectPalette();
      palette_obj.MapId = -1;
      palette_obj.Name = name.toStdString();
      for (auto& path : _object_paths)
        palette_obj.Filepaths.push_back(path);

      _project->saveNamedObjectPalette(palette_obj);

      // show the palette as loaded, so further work continues on it
      refreshPaletteSelector(name);
    }

    void ObjectPalette::deleteSelectedNamedPalette()
    {
      QString const name = _palette_selector->currentData().toString();

      if (name.isEmpty())
      {
        // the per-map palette cannot be deleted from here
        return;
      }

      if (QMessageBox::question(this
          , tr("Delete palette")
          , tr("Delete palette '%1'?").arg(name)) != QMessageBox::Yes)
      {
        return;
      }

      _project->deleteNamedObjectPalette(name.toStdString());
      refreshPaletteSelector();
    }

    void ObjectPalette::exportPaletteToFile()
    {
      QString name = _palette_selector->currentData().toString();
      if (name.isEmpty())
      {
        name = "palette";
      }

      QString const filepath = QFileDialog::getSaveFileName(this
        , tr("Export palette")
        , name + ".noggitpalette"
        , tr("Noggit palette (*.noggitpalette *.json)"));

      if (filepath.isEmpty())
        return;

      QFile out_file(filepath);
      if (!out_file.open(QIODevice::WriteOnly | QFile::Truncate))
      {
        LogError << "Unable to export palette to " << filepath.toStdString() << std::endl;
        QMessageBox::warning(this, tr("Export palette"), tr("Could not write the file."));
        return;
      }

      QJsonObject root;
      QJsonArray filepaths;
      for (auto& path : _object_paths)
        filepaths.push_back(path.c_str());

      root.insert("Name", name);
      root.insert("Filepaths", filepaths);

      out_file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
      out_file.close();
    }

    void ObjectPalette::importPaletteFromFile()
    {
      QString const filepath = QFileDialog::getOpenFileName(this
        , tr("Import palette")
        , QString()
        , tr("Noggit palette (*.noggitpalette *.json);;All files (*)"));

      if (filepath.isEmpty())
        return;

      QFile in_file(filepath);
      if (!in_file.open(QIODevice::ReadOnly))
      {
        LogError << "Unable to import palette from " << filepath.toStdString() << std::endl;
        QMessageBox::warning(this, tr("Import palette"), tr("Could not read the file."));
        return;
      }

      auto const json_doc = QJsonDocument::fromJson(in_file.readAll());
      in_file.close();

      if (!json_doc.isObject())
      {
        LogError << "Invalid palette file " << filepath.toStdString() << std::endl;
        QMessageBox::warning(this, tr("Import palette"), tr("The file is not a valid palette."));
        return;
      }

      QJsonObject root = json_doc.object();

      QString name = root.value("Name").toString().trimmed();
      if (name.isEmpty())
      {
        name = QFileInfo(filepath).completeBaseName();
      }

      auto palette_obj = Noggit::Project::NoggitProjectObjectPalette();
      palette_obj.MapId = -1;
      palette_obj.Name = name.toStdString();

      auto const json_filepaths = root.value("Filepaths").toArray();
      for (auto const& json_filepath : json_filepaths)
      {
        palette_obj.Filepaths.push_back(json_filepath.toString().toStdString());
      }

      // store it with the saved palettes and load it right away
      _project->saveNamedObjectPalette(palette_obj);
      refreshPaletteSelector(name);

      clearPaletteObjects();
      for (auto const& filename : palette_obj.Filepaths)
      {
        addObjectByFilename(filename.c_str(), false);
      }
    }

    void ObjectPalette::addObjectFromAssetBrowser()
    {

      std::string const& display_name = reinterpret_cast<Noggit::Ui::Tools::AssetBrowser::Ui::AssetBrowserWidget*>(
          _map_view->getAssetBrowser()->widget())->getFilename();

      addObjectByFilename(display_name.c_str());
    }


    void ObjectPalette::removeObject(QString filename)
    {

      QList<QListWidgetItem*> objects = _object_list->findItems(filename, Qt::MatchExactly);

      for (auto obj: objects)
        if (obj->toolTip() == filename)
        {
          _object_paths.erase(filename.toStdString());
          _object_list->removeItemWidget(obj);
          _add_button->setDisabled(false);
          delete obj;
          return;
        }


    }

    void ObjectPalette::removeSelectedTexture()
    {

      QList<QListWidgetItem*> selected_items = _object_list->selectedItems();

      for (auto item: selected_items)
      {

        for (auto path: _object_paths)
          if (path == item->toolTip().toStdString())
          {
            _object_paths.erase(path);
            _object_list->removeItemWidget(item);
            _add_button->setDisabled(false);
            delete item;
            return;
          }

      }
    }

    void ObjectPalette::dragEnterEvent(QDragEnterEvent* event)
    {
      if (event->mimeData()->hasText()
          && (_object_paths.find(event->mimeData()->text().toStdString()) == _object_paths.end())
          )
        event->accept();
    }

    void ObjectPalette::addObjectByFilename(QString const& filename, bool save_palette)
    {
      if (filename.isEmpty())
        return;

      for (auto path: _object_paths)
        if (path == filename.toStdString())
          return;

      _object_paths.emplace(filename.toStdString());

      QListWidgetItem* list_item = new QListWidgetItem(_object_list);
      _preview_renderer->setModelOffscreen(filename.toStdString());
      list_item->setIcon(*_preview_renderer->renderToPixmap());
      list_item->setData(Qt::DisplayRole, filename);
      list_item->setToolTip(filename);
      list_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
      list_item->setText("");

      _object_list->addItem(list_item);

      // auto saving whenever an object is added, could change it to manually save
      if (save_palette)
        SavePalette();
    }

    void ObjectPalette::dropEvent(QDropEvent* event)
    {
      addObjectByFilename(event->mimeData()->text());
      event->accept();
    }

    ObjectPalette::~ObjectPalette()
    {
      delete _preview_renderer;
    }

  }

}
