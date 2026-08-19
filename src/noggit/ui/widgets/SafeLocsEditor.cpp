// This file is part of Noggit3, licensed under GNU General Public License (version 3).
// Fix for issue #52: graveyard editor for WorldSafeLocs.dbc.

#include <noggit/ui/widgets/SafeLocsEditor.hpp>

#include <noggit/DBC.h>
#include <noggit/Log.h>
#include <noggit/MapView.h>
#include <noggit/World.h>
#include <noggit/ui/FontAwesome.hpp>

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

namespace Noggit::Ui
{
  static const unsigned int first_location_id = 100000;

  // tree columns match the WorldSafeLocsDB field indexes
  static const int id_column = 0;
  static const int map_column = 1;
  static const int first_coordinate_column = 2;

  SafeLocsEditor::SafeLocsEditor(MapView* map_view)
    : QDialog(map_view, Qt::Window)
    , _map_view(map_view)
  {
    setWindowTitle(tr("World Safe Locations (graveyards)"));
    setModal(false);

    // the dialog may outlive the MapView
    connect(map_view, &QObject::destroyed, this, [this]()
      {
        _map_view_alive = false;
        _map_view = nullptr;
      });

    _layout = new QVBoxLayout(this);

    // the dbc may not be available (file missing from the client archives)
    if (!gWorldSafeLocsDB.getRecordSize())
    {
      auto* error_label = new QLabel(tr("WorldSafeLocs.dbc could not be loaded from the client data."), this);
      error_label->setWordWrap(true);
      _layout->addWidget(error_label);

      auto* error_button_box = new QDialogButtonBox(this);
      _close_button = error_button_box->addButton(QDialogButtonBox::Close);
      _layout->addWidget(error_button_box);

      connect(_close_button, &QPushButton::clicked, this, &QDialog::reject);
      return;
    }

    auto* hint_label = new QLabel(this);
    hint_label->setText(tr(
      "Graveyard locations of WorldSafeLocs.dbc. Double-click coordinates to edit them.\n"
      "Modified records are marked with * and written the next time the map is saved."));
    hint_label->setWordWrap(true);
    _layout->addWidget(hint_label);

    _current_map_only = new QCheckBox(tr("Only show locations of the loaded map"), this);
    _layout->addWidget(_current_map_only);

    _tree = new QTreeWidget(this);
    _tree->setColumnCount(5);
    _tree->setHeaderLabels({tr("ID"), tr("Map"), tr("X"), tr("Y"), tr("Z")});
    _tree->setRootIsDecorated(false);
    _tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _tree->setUniformRowHeights(true);
    _tree->header()->setSectionResizeMode(id_column, QHeaderView::ResizeToContents);
    _tree->header()->setSectionResizeMode(map_column, QHeaderView::Stretch);
    _layout->addWidget(_tree);

    auto* button_layout = new QHBoxLayout();
    _layout->addLayout(button_layout);

    auto* add_at_cursor_button = new QPushButton(tr("Add at cursor position"), this);
    add_at_cursor_button->setIcon(FontAwesomeIcon(FontAwesome::mapmarker));
    button_layout->addWidget(add_at_cursor_button);

    auto* add_at_camera_button = new QPushButton(tr("Add at camera position"), this);
    add_at_camera_button->setIcon(FontAwesomeIcon(FontAwesome::video));
    button_layout->addWidget(add_at_camera_button);

    auto* teleport_button = new QPushButton(tr("Teleport here"), this);
    teleport_button->setIcon(FontAwesomeIcon(FontAwesome::rocket));
    button_layout->addWidget(teleport_button);

    auto* remove_button = new QPushButton(tr("Remove selected"), this);
    remove_button->setIcon(FontAwesomeIcon(FontAwesome::times));
    button_layout->addWidget(remove_button);

    button_layout->addStretch(1);

    auto* bottom_layout = new QHBoxLayout();
    _layout->addLayout(bottom_layout);

    _status_label = new QLabel(this);
    bottom_layout->addWidget(_status_label, 1);

    auto* button_box = new QDialogButtonBox(this);
    _save_button = button_box->addButton(QDialogButtonBox::Save);
    _close_button = button_box->addButton(QDialogButtonBox::Close);
    bottom_layout->addWidget(button_box);

    refill();
    apply_modified_flag(false);

    connect(_current_map_only, &QCheckBox::toggled, this, [this](bool) { refill(); });

    connect(add_at_cursor_button, &QPushButton::clicked, this, [this]()
      {
        if (!_map_view_alive)
        {
          return;
        }

        glm::vec3 const pos = _map_view->cursorPosition();
        add_location(_map_view->getWorld()->getMapID(), pos.x, pos.y, pos.z);
      }
    );

    connect(add_at_camera_button, &QPushButton::clicked, this, [this]()
      {
        if (!_map_view_alive)
        {
          return;
        }

        glm::vec3 const pos = _map_view->_camera.position;
        add_location(_map_view->getWorld()->getMapID(), pos.x, pos.y, pos.z);
      }
    );

    connect(teleport_button, &QPushButton::clicked, this, [this]() { teleport_to_selected(); });
    connect(remove_button, &QPushButton::clicked, this, [this]() { remove_selected_locations(); });
    connect(_save_button, &QPushButton::clicked, this, [this]() { save_changes(); });
    connect(_close_button, &QPushButton::clicked, this, &QDialog::reject);

    connect(_tree, &QTreeWidget::itemChanged
      , [this](QTreeWidgetItem* item, int column)
        {
          auto it = _record_ids.find(item);
          if (it == _record_ids.end())
          {
            return;
          }

          // reverting/restoring cell texts must not recurse into this handler
          QSignalBlocker const blocker (_tree);

          try
          {
            DBCFile::Record record = gWorldSafeLocsDB.getByID(it->second, WorldSafeLocsDB::ID);

            // only the coordinate columns are editable, revert everything else
            if (column < first_coordinate_column)
            {
              restore_cell(item, column, record);
              return;
            }

            bool conversion_ok = false;
            float const value = item->text(column).toFloat(&conversion_ok);

            if (!conversion_ok)
            {
              restore_cell(item, column, record);
              return;
            }

            record.write(static_cast<std::size_t>(column), value);
            mark_item_modified(item);
            apply_modified_flag(true);

            if (_map_view_alive)
            {
              _map_view->setDbcDirty(&gWorldSafeLocsDB);
            }
          }
          catch (...)
          {
            LogError << "unable to edit the WorldSafeLocs.dbc record" << std::endl;
          }
        }
    );
  }

  void SafeLocsEditor::restore_cell(QTreeWidgetItem* item, int column, DBCFile::Record& record)
  {
    unsigned int const id = record.getUInt(WorldSafeLocsDB::ID);

    switch (column)
    {
    case id_column:
      item->setText(id_column
        , QString::number(id) + (item->text(id_column).endsWith("*") ? "*" : ""));
      break;
    case map_column:
      {
        int const map_id = record.getInt(WorldSafeLocsDB::Continent);
        item->setText(map_column
          , QString::fromStdString(MapDB::getMapName(map_id)) + " (" + QString::number(map_id) + ")");
      }
      break;
    default:
      item->setText(column, QString::number(record.getFloat(column)));
      break;
    }
  }

  void SafeLocsEditor::mark_item_modified(QTreeWidgetItem* item)
  {
    unsigned int const id = _record_ids.at(item);
    item->setText(id_column, QString::number(id) + "*");
  }

  void SafeLocsEditor::add_record_row(DBCFile::Record& record)
  {
    unsigned int const id = record.getUInt(WorldSafeLocsDB::ID);
    int const map_id = record.getInt(WorldSafeLocsDB::Continent);

    if (_current_map_only->isChecked()
        && _map_view_alive
        && map_id != static_cast<int>(_map_view->getWorld()->getMapID()))
    {
      return;
    }

    auto* item = new QTreeWidgetItem(_tree);
    _record_ids[item] = id;

    item->setText(id_column, QString::number(id));
    item->setText(map_column
      , QString::fromStdString(MapDB::getMapName(map_id)) + " (" + QString::number(map_id) + ")");
    item->setText(WorldSafeLocsDB::LocX, QString::number(record.getFloat(WorldSafeLocsDB::LocX)));
    item->setText(WorldSafeLocsDB::LocY, QString::number(record.getFloat(WorldSafeLocsDB::LocY)));
    item->setText(WorldSafeLocsDB::LocZ, QString::number(record.getFloat(WorldSafeLocsDB::LocZ)));

    item->setFlags(item->flags() | Qt::ItemIsEditable);
  }

  void SafeLocsEditor::refill()
  {
    _record_ids.clear();
    _tree->clear();

    for (DBCFile::Iterator i = gWorldSafeLocsDB.begin(); i != gWorldSafeLocsDB.end(); ++i)
    {
      add_record_row(*i);
    }
  }

  unsigned int SafeLocsEditor::find_free_id() const
  {
    unsigned int new_id = first_location_id;

    for (DBCFile::Iterator i = gWorldSafeLocsDB.begin(); i != gWorldSafeLocsDB.end(); ++i)
    {
      new_id = std::max(new_id, i->getUInt(WorldSafeLocsDB::ID) + 1);
    }

    return new_id;
  }

  void SafeLocsEditor::add_location(int map_id, float x, float y, float z)
  {
    unsigned int const new_id = find_free_id();

    try
    {
      DBCFile::Record record = gWorldSafeLocsDB.addRecord(new_id, WorldSafeLocsDB::ID);
      record.write(WorldSafeLocsDB::Continent, map_id);
      record.write(WorldSafeLocsDB::LocX, x);
      record.write(WorldSafeLocsDB::LocY, y);
      record.write(WorldSafeLocsDB::LocZ, z);
      record.writeString(WorldSafeLocsDB::Comment, "Noggit safe location");

      auto* item = new QTreeWidgetItem(_tree);
      _record_ids[item] = new_id;

      item->setText(id_column, QString::number(new_id) + "*");
      item->setText(map_column
        , QString::fromStdString(MapDB::getMapName(map_id)) + " (" + QString::number(map_id) + ")");
      item->setText(WorldSafeLocsDB::LocX, QString::number(x));
      item->setText(WorldSafeLocsDB::LocY, QString::number(y));
      item->setText(WorldSafeLocsDB::LocZ, QString::number(z));
      item->setFlags(item->flags() | Qt::ItemIsEditable);
      _tree->setCurrentItem(item);
      _tree->scrollToItem(item);

      apply_modified_flag(true);

      if (_map_view_alive)
      {
        _map_view->setDbcDirty(&gWorldSafeLocsDB);
      }
    }
    catch (...)
    {
      LogError << "unable to add a record to WorldSafeLocs.dbc" << std::endl;
    }
  }

  void SafeLocsEditor::remove_selected_locations()
  {
    auto const selection = _tree->selectedItems();
    if (selection.isEmpty())
    {
      return;
    }

    bool removed_any = false;

    for (auto* item : selection)
    {
      auto it = _record_ids.find(item);
      if (it == _record_ids.end())
      {
        continue;
      }

      try
      {
        gWorldSafeLocsDB.removeRecord(it->second, WorldSafeLocsDB::ID);
        _record_ids.erase(it);
        delete item;
        removed_any = true;
      }
      catch (...)
      {
        LogError << "unable to remove the WorldSafeLocs.dbc record" << std::endl;
      }
    }

    if (removed_any)
    {
      apply_modified_flag(true);

      if (_map_view_alive)
      {
        _map_view->setDbcDirty(&gWorldSafeLocsDB);
      }
    }
  }

  void SafeLocsEditor::teleport_to_selected()
  {
    if (!_map_view_alive)
    {
      return;
    }

    auto const selection = _tree->selectedItems();
    if (selection.isEmpty())
    {
      return;
    }

    auto it = _record_ids.find(selection.first());
    if (it == _record_ids.end())
    {
      return;
    }

    try
    {
      DBCFile::Record record = gWorldSafeLocsDB.getByID(it->second, WorldSafeLocsDB::ID);

      // hover a little above the actual location
      _map_view->_camera.position = glm::vec3
        ( record.getFloat(WorldSafeLocsDB::LocX)
        , record.getFloat(WorldSafeLocsDB::LocY) + 10.f
        , record.getFloat(WorldSafeLocsDB::LocZ)
        );
    }
    catch (...)
    {
      LogError << "unable to teleport to the WorldSafeLocs.dbc record" << std::endl;
    }
  }

  void SafeLocsEditor::save_changes()
  {
    gWorldSafeLocsDB.save();
    _modified = false;

    _status_label->setText(tr("WorldSafeLocs.dbc saved to the project folder."));
    _save_button->setVisible(false);

    // drop the * markers
    for (int i (0); i < _tree->topLevelItemCount(); ++i)
    {
      QTreeWidgetItem* item = _tree->topLevelItem(i);
      QString id_text = item->text(id_column);

      if (id_text.endsWith("*"))
      {
        id_text.chop(1);
        item->setText(id_column, id_text);
      }
    }
  }

  void SafeLocsEditor::apply_modified_flag(bool modified)
  {
    _modified = modified;

    _save_button->setVisible(_modified);
    _status_label->setText
      ( _modified
      ? tr("Unsaved changes (they will also be written with the next map save).")
      : QString()
      );
  }
}
