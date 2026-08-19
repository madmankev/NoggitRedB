// This file is part of Noggit3, licensed under GNU General Public License (version 3).
#pragma once

#include <noggit/DBC.h>

#include <QDialog>

#include <algorithm>
#include <unordered_map>

class QCheckBox;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class MapView;

namespace Noggit::Ui
{
  // Fix for issue #52 (WorldSafeLocs.dbc (graveyards) editor): table editor
  // for the WorldSafeLocs.dbc client database. Entries can be added at the
  // current cursor/camera position, edited in place and removed again.
  // Modifications are flushed to the project folder together with the map
  // save (see ``*`` markers) or immediately with the Save button.
  class SafeLocsEditor : public QDialog
  {
    Q_OBJECT

  public:
    explicit SafeLocsEditor(MapView* map_view);

    QSize sizeHint() const override { return {560, 480}; }

  private:
    void add_location(int map_id, float x, float y, float z);
    void remove_selected_locations();
    void save_changes();
    void teleport_to_selected();

    void refill();
    void add_record_row(DBCFile::Record& record);
    void apply_modified_flag(bool modified);
    void restore_cell(QTreeWidgetItem* item, int column, DBCFile::Record& record);
    void mark_item_modified(QTreeWidgetItem* item);
    unsigned int find_free_id() const;

    MapView* _map_view;
    bool _map_view_alive = true;

    QVBoxLayout* _layout;
    QTreeWidget* _tree;
    QCheckBox* _current_map_only;
    QLabel* _status_label;
    QPushButton* _save_button;
    QPushButton* _close_button;

    bool _modified = false;

    // tree items are re-created on refill; locate the record of an item
    std::unordered_map<QTreeWidgetItem*, unsigned int> _record_ids;
  };
}
