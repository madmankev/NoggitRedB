// This file is part of Noggit3, licensed under GNU General Public License (version 3).
#include "TaxiEditor.hpp"
#include <cmath>


#include <noggit/DBC.h>
#include <noggit/Log.h>
#include <noggit/MapView.h>

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QFormLayout>

namespace Noggit
{
  namespace Ui
  {
      TaxiEditor::TaxiEditor(MapView* map_view, QWidget* parent)
          : QWidget(parent)
          , _taxi_nodes_tree(new QTreeWidget())
          , mapID(map_view->getWorld()->getMapID())
    {
          auto layout = new QFormLayout(this);
          _taxi_nodes_tree->setHeaderLabel("Taxi Nodes.\n Children are taxi paths destinations.");
          buildTaxiNodesList();
          // auto layout = new QVBoxLayout(this);

          layout->addRow(_taxi_nodes_tree);


          setMinimumWidth(700);

    }
      void TaxiEditor::buildTaxiNodesList()
      {
          QSignalBlocker const block_area_tree(_taxi_nodes_tree);
          _taxi_nodes_tree->clear();
          _taxi_nodes_tree->setColumnCount(1);
          _items.clear();

          //  Read out Nodes List.
          for (DBCFile::Iterator i = gTaxiNodesDB.begin(); i != gTaxiNodesDB.end(); ++i)
          {
              if (i->getInt(TaxiNodesDB::MapId) == mapID)
              {
                  add_taxi_node_item(i->getInt(TaxiNodesDB::ID));
              }
          }
      }

      QTreeWidgetItem* TaxiEditor::add_taxi_node_item(int node_id)
      {
          QTreeWidgetItem* item = create_or_get_tree_widget_item(node_id);
          _taxi_nodes_tree->addTopLevelItem(item);

          int path_count = 0;
          // add paths with this node as the "FROM" as children
          for (DBCFile::Iterator i = gTaxiPathDB.begin(); i != gTaxiPathDB.end(); ++i)
          {
              if (i->getInt(TaxiPathDB::FromTaxiNode) == node_id)
              {
                  int pathid = i->getInt(TaxiPathDB::ID);// debug
                  // auto taxipath_rec = gTaxiPathDB.getByID(i->getInt(TaxiPathDB::ID));

                  if (i->getInt(TaxiPathDB::ToTaxiNode) == -1)
                      continue; // some have -1, figure out wat it's for

                  try
                  {
                    auto destnode_rec = gTaxiNodesDB.getByID(i->getInt(TaxiPathDB::ToTaxiNode));
                    // QTreeWidgetItem* path_item = add_area(parent_area_id);
                    QTreeWidgetItem* path_item = new QTreeWidgetItem();

                    path_item->setData(0, 1, QVariant(i->getInt(TaxiPathDB::ID))); // Store the path id in the item
                    path_item->setText(0, QString(destnode_rec.getLocalizedString(TaxiNodesDB::Name)));

                    item->addChild(path_item);
                    path_count++;
                  }
                  catch (TaxiNodesDB::NotFound)
                  {
                  }
              }
          }
          std::stringstream ss;
          ss << item->text(0).toStdString() << " (" << path_count << " paths)";
          item->setText(0, ss.str().c_str());

          return item;
      }

      QTreeWidgetItem* TaxiEditor::create_or_get_tree_widget_item(int node_id)
      {
          auto it = _items.find(node_id);

          if (it != _items.end())
          {
              return _items.at(node_id);
          }
          else
          {
              QTreeWidgetItem* item = new QTreeWidgetItem();

              std::stringstream ss;
              std::string nodeName = "";
              try
              {
                  TaxiNodesDB::Record rec = gTaxiNodesDB.getByID(node_id);
                  nodeName = rec.getLocalizedString(TaxiNodesDB::Name);
              }
              catch (TaxiNodesDB::NotFound)
              {
              }
              ss << node_id << "-" << nodeName;
              item->setData(0, 1, QVariant(node_id));
              item->setText(0, QString(ss.str().c_str()));
              _items.emplace(node_id, item);

              return item;
          }
      }
  }
}
