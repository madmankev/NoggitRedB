// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#ifndef NOGGIT_OBJECT_PALETTE_HPP
#define NOGGIT_OBJECT_PALETTE_HPP

#include <noggit/ui/widget.hpp>

#include <QtWidgets/QListWidget>

#include <string>
#include <unordered_set>

class QGridLayout;
class QPushButton;
class QComboBox;
class QDropEvent;
class QDragEnterEvent;
class QMouseEvent;
class QListWidget;
class QPoint;
class MapView;

namespace Noggit::Project
{
  class NoggitProject;
}

namespace Noggit
{
  namespace Ui
  {
    namespace Tools
    {
      class PreviewRenderer;
    }

    class current_texture;

    class ObjectList : public QListWidget
    {
    public:
      ObjectList(QWidget* parent);
      void mouseMoveEvent(QMouseEvent* event) override;
      void mousePressEvent(QMouseEvent* event) override;

    private:
      QPoint _start_pos;

    };

    class ObjectPalette : public widget
    {
      Q_OBJECT

    public:
      ObjectPalette(MapView* map_view, std::shared_ptr<Noggit::Project::NoggitProject> Project, QWidget* parent);

      ~ObjectPalette();

      void addObjectFromAssetBrowser();
      void addObjectByFilename(QString const& filename, bool save_palette = true);
      void LoadSavedPalette();

      void SavePalette();

      // Fix for issue #36: named palettes that can be saved, loaded, deleted and
      // exported/imported as files to share with other designers or workstations.
      void saveCurrentPaletteAsNamed();
      void loadNamedPalette(int index);
      void deleteSelectedNamedPalette();
      void exportPaletteToFile();
      void importPaletteFromFile();

      void removeObject(QString filename);

      void removeSelectedTexture();

      void dragEnterEvent(QDragEnterEvent* event) override;
      void dropEvent(QDropEvent* event) override;

    signals:
      void selected(std::string);

    private:

      QGridLayout* layout;

      ObjectList* _object_list;
      QPushButton* _add_button;
      QPushButton* _remove_button;
      // Fix for issue #36: named palette controls
      QComboBox* _palette_selector;
      QPushButton* _save_named_button;
      QPushButton* _delete_named_button;
      QPushButton* _export_button;
      QPushButton* _import_button;
      std::unordered_set<std::string> _object_paths;
      MapView* _map_view;
      Noggit::Ui::Tools::PreviewRenderer* _preview_renderer;
      std::shared_ptr<Noggit::Project::NoggitProject> _project;

      // Fix for issue #36: refill the named palette dropdown, keeping the selection when possible.
      void refreshPaletteSelector(QString const& select_name = QString());
      // Fix for issue #36: remove all objects from the list without touching saved palettes.
      void clearPaletteObjects();

    };
  }
}

#endif //NOGGIT_OBJECT_PALETTE_HPP
