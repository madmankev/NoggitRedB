// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QSlider>
#include <QtWidgets/QCheckBox.h>
#include <QtWidgets/QComboBox.h>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QListView>
#include <QtWidgets/QPushButton>

#include <functional>
#include <string>

class MapView;

namespace Noggit
{
    namespace Ui
    {
        class TaxiEditor : public QWidget
        {
            Q_OBJECT

        public:
            TaxiEditor(MapView* map_view, QWidget* parent = nullptr);



        private:
            QTreeWidget* _taxi_nodes_tree;
            std::map<int, QTreeWidgetItem*> _items;

            void buildTaxiNodesList();

            QTreeWidgetItem* add_taxi_node_item(int node_id);

            QTreeWidgetItem* create_or_get_tree_widget_item(int node_id);

            int mapID;
        };
    }
}

