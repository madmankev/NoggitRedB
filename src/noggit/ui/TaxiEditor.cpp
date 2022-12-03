// This file is part of Noggit3, licensed under GNU General Public License (version 3).
#include "TaxiEditor.hpp"
#include <cmath>
#include <noggit/Selection.h>

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
          , _map_view(map_view)
    {
          auto layout = new QFormLayout(this);
          _taxi_nodes_tree->setHeaderLabel("Taxi Nodes.\n Children are taxi paths destinations.");
          _taxi_nodes_tree->setSelectionMode(QAbstractItemView::SingleSelection);
          buildTaxiNodesList();
          // auto layout = new QVBoxLayout(this);

          layout->addRow(_taxi_nodes_tree);


          setMinimumWidth(250);

          connect(_taxi_nodes_tree, &QTreeWidget::itemSelectionChanged
              , [this]
              {
                current_path = nullptr;
                _map_view->getWorld()->renderer()->setTaxiPath(nullptr);

                auto const& selected_items = _taxi_nodes_tree->selectedItems();
                if (selected_items.size())
                {
                    auto selected_item = selected_items.back();
                
                    selected_path_id = 0;
                    selected_node_id = 0;
                    // check if is toplevel (node) or child (path)
                    if (!selected_item->parent())
                    { // is taxi node
                        // selected_node_id = selected_item->data(0, 1).toInt();
                    }
                    else
                    {
                        // is taxi path
                        set_current_path(selected_item->data(0, 1).toInt());
                
                        // selected_path_id = selected_item->data(0, 1).toInt();
                        // selected_node_id = selected_item->parent()->data(0, 1).toInt(); // select the parent node ?
                    }
                
                    // emit selected(selected_items.back()->data(0, 1).toInt());
                }
              });
    }
      void TaxiEditor::buildTaxiNodesList()
      {
          QSignalBlocker const block_area_tree(_taxi_nodes_tree);
          _taxi_nodes_tree->clear();
          _taxi_nodes_tree->setColumnCount(1);
          _items.clear();

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
      
      TaxiPath* TaxiEditor::set_current_path(int path_id)
      {
          for (auto& taxi_node : _map_view->getWorld()->renderer()->taxis()->taxiNodes)
          {
              for (TaxiPath& path : taxi_node.taxiPaths)
              {
                  if (path.Id == path_id)
                  {
                      current_path = &path;
                      _map_view->getWorld()->renderer()->setTaxiPath(current_path);
                      return current_path;
                  }

              }
          }
          current_path = nullptr;
          _map_view->getWorld()->renderer()->setTaxiPath(nullptr);
          return current_path;
      }
  }
}

Taxis::Taxis(unsigned int mapid, Noggit::NoggitRenderContext context)
    : _context(context)
{
    for (DBCFile::Iterator i = gTaxiNodesDB.begin(); i != gTaxiNodesDB.end(); ++i)
    {
        if (mapid == i->getUInt(TaxiNodesDB::MapId))
        {
            // TaxiNode taxiNode(i, _context);
            taxiNodes.emplace_back(i, _context);
            // numSkies++;
        }
    }
}

TaxiNode::TaxiNode(DBCFile::Iterator data, Noggit::NoggitRenderContext context)
    : SceneObject(SceneObjectTypes::eTaxi_Node, context)
{
    Id = data->getInt(TaxiNodesDB::ID);
    MapId = data->getInt(TaxiNodesDB::MapId);
    // convert to client coords from server coords

    pos = glm::vec3(ZEROPOINT - data->getFloat(TaxiNodesDB::PositionY),
        data->getFloat(TaxiNodesDB::PositionZ),
        ZEROPOINT - data->getFloat(TaxiNodesDB::PositionX));

    Name = data->getLocalizedString(TaxiNodesDB::Name);
    CreatureMountIdAlliance = data->getInt(TaxiNodesDB::MountCreatureIdAlliance);
    CreatureMountIdHorde = data->getInt(TaxiNodesDB::MountCreatureIdHorde);

    // pos = glm::vec3(0.0f, 0.0f, 0.0f);
    // dir = math::degrees::vec3(math::degrees(0)._, math::degrees(0)._, math::degrees(0)._);
    // uid = 0;
    _context = context;
    scale = 5.0f;

    recalcExtents();

    updateTransformMatrix();

    // store all paths that start from this node
    for (DBCFile::Iterator i = gTaxiPathDB.begin(); i != gTaxiPathDB.end(); ++i)
    {
        if (i->getInt(TaxiPathDB::FromTaxiNode) == Id)
        {
            //  TaxiPath taxiPath(i);
            // auto taxiPath = new TaxiPath(i);
            taxiPaths.emplace_back(i, context);
        }
    }
}

void TaxiNode::draw(glm::mat4x4 mvp, glm::vec3 camera_pos, float cull_distance)
{
    if (glm::distance(pos, camera_pos) <= cull_distance)
    {
        glm::vec4 color = { 0.0f, 1.0f, 0.0f, 1.f };

        // _sphere_render.draw(mvp, pos, color, scale, 32, 18, 0.8f, false, true);

        // AWFUL PERFORMANCE DONT DO THIS
        // auto sphere = Noggit::Rendering::Primitives::Sphere();
        // sphere.draw(mvp, pos, color, scale, 32, 18, 0.6f, false, false); // getInstance(_context).draw
    }
}

void TaxiPathNode::draw(glm::mat4x4 mvp, glm::vec3 camera_pos, float cull_distance)
{
    if (glm::distance(pos, camera_pos) <= cull_distance)
    {
        glm::vec4 color = { 1.0f, 0.5f, 0.0f, 1.f };// orange

        // _sphere_render.draw(mvp, pos, color, scale, 32, 18, 0.8f, false, true);

        // AWFUL PERFORMANCE DONT DO THIS
        // auto sphere = Noggit::Rendering::Primitives::Sphere();
        // sphere.draw(mvp, pos, color, scale, 32, 18, 0.8f, false, false); // getInstance(_context).draw
    }
}

void TaxiPathNode::recalcExtents()
{
    updateTransformMatrix();

    extents[0] = glm::vec3(pos.x - scale, pos.y - scale, pos.z - scale);
    extents[1] = glm::vec3(pos.x + scale, pos.y + scale, pos.z + scale);
}

void TaxiPathNode::ensureExtents()
{
    recalcExtents();
}

void TaxiPathNode::updateDetails(Noggit::Ui::detail_infos* detail_widget)
{
    std::stringstream select_info;

    select_info << "<b>TaxiPathNode.dbc id: </b>" << Id
        // << "<b>TaxiPath.dbc id: </b>" << path
        << "<br><b>Node Index: </b>" << NodeIndex
        << "<br><b>position X/Y/Z: </b>{" << pos.x << ", " << pos.y << ", " << pos.z << "}"
        << "<br><b>Delay: </b>" << Delay;

    select_info << "<br></span>";

    detail_widget->setText(select_info.str());
}

void TaxiNode::recalcExtents()
{
    updateTransformMatrix();

    extents[0] = glm::vec3(pos.x - scale, pos.y - scale, pos.z - scale );
    extents[1] = glm::vec3(pos.x + scale, pos.y + scale, pos.z + scale);
}

void TaxiNode::ensureExtents()
{
    recalcExtents();
}

void TaxiNode::updateDetails(Noggit::Ui::detail_infos* detail_widget)
{
    std::stringstream select_info;

    select_info << "<b>TaxiNodes.dbc id: </b>" << Id
        << "<br><b>Name: </b>" << Name
        << "<br><b>position X/Y/Z: </b>{" << pos.x << ", " << pos.y << ", " << pos.z << "}"
        << "<br><b>Number of paths: </b>" << taxiPaths.size();

    select_info << "<br></span>";

    detail_widget->setText(select_info.str());
}

TaxiPath::TaxiPath(DBCFile::Iterator data, Noggit::NoggitRenderContext context)
{
    Id = data->getInt(TaxiPathDB::ID);
    starting_node_id = data->getInt(TaxiPathDB::FromTaxiNode);
    destination_node_id = data->getInt(TaxiPathDB::ToTaxiNode);
    Cost = data->getInt(TaxiPathDB::CopperCost);

    // get path nodes
    for (DBCFile::Iterator i = gTaxiPathNodeDB.begin(); i != gTaxiPathNodeDB.end(); ++i)
    {
        if (i->getInt(TaxiPathNodeDB::PathId) == Id)
        {
            //TaxiPathNode taxiPathNode(i);
            PathNodes.emplace_back(i, context);
        }
    }
}

TaxiPath::TaxiPath(unsigned int path_id, Noggit::NoggitRenderContext context)
{
    auto rec = gTaxiPathDB.getByID(path_id);

    Id = rec.getInt(TaxiPathDB::ID);
    starting_node_id = rec.getInt(TaxiPathDB::FromTaxiNode);
    destination_node_id = rec.getInt(TaxiPathDB::ToTaxiNode);
    Cost = rec.getInt(TaxiPathDB::CopperCost);

    // get path nodes
    for (DBCFile::Iterator i = gTaxiPathNodeDB.begin(); i != gTaxiPathNodeDB.end(); ++i)
    {
        if (i->getInt(TaxiPathNodeDB::PathId) == Id)
        {
            //TaxiPathNode taxiPathNode(i);
            PathNodes.emplace_back(i, context);
        }
    }
}

void TaxiPath::drawPathNodes(glm::mat4x4 mvp, glm::vec3 camera_pos, float cull_distance)
{
    for (auto& path_node : PathNodes)
    {
        path_node.draw(mvp, camera_pos, cull_distance);
    }
}

TaxiPathNode::TaxiPathNode(DBCFile::Iterator data, Noggit::NoggitRenderContext context)
    : SceneObject(SceneObjectTypes::eTaxi_Node, context)
{
    Id = data->getInt(TaxiPathNodeDB::ID);
    // int PathId; // TaxiPath Id
    NodeIndex = data->getInt(TaxiPathNodeDB::NodeIndex);
    MapId = data->getInt(TaxiPathNodeDB::ContinentId);
    // convert to client coords from server coords
    pos = glm::vec3(ZEROPOINT - data->getFloat(TaxiPathNodeDB::PositionY),
        data->getFloat(TaxiPathNodeDB::PositionZ),
        ZEROPOINT - data->getFloat(TaxiPathNodeDB::PositionX));
    // int Flags;
    Delay = data->getInt(TaxiPathNodeDB::Delay);
    ArrivalEventId = data->getInt(TaxiPathNodeDB::ArrivalEventId);
    DepartureEventId = data->getInt(TaxiPathNodeDB::DepartureEventId);

    _context = context;
    scale = 4.0f;

    recalcExtents();

    updateTransformMatrix();
}

void TaxiPathNode::intersect(math::ray const& ray, selection_result* results)
{
    auto bounds_intersect = ray.intersect_bounds(extents[0], extents[1]);

    // very simple bound intersect (square) TODO : sphere intersect
    if (bounds_intersect.has_value())
    {
        results->emplace_back(bounds_intersect.value(), this);
    }
}

