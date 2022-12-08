// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#pragma once
#include <noggit/DBCFile.h>
#include <noggit/ContextObject.hpp>
#include <noggit/rendering/Primitives.hpp>
#include <external/glm/glm.hpp>
#include <opengl/scoped.hpp>
#include <opengl/shader.fwd.hpp>
#include <noggit/SceneObject.hpp>

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
#include <vector>
#include <math/ray.hpp>

class MapView;


class TaxiPathNode : public GenericSelectableObject
{
public:
    TaxiPathNode(DBCFile::Iterator data, Noggit::NoggitRenderContext context);

    void intersect(math::ray const& ray, selection_result* results);
    // glm::vec3 Position;

    void draw(glm::mat4x4 mvp, glm::vec3 camera_pos, float cull_distance);

    glm::vec3 position() { return pos; };

    void recalcExtents() override;
    void ensureExtents() override;
    virtual void updateDetails(Noggit::Ui::detail_infos* detail_widget) override;

    int Id;
    int NodeIndex;
    int Delay;
    int ArrivalEventId;
    int DepartureEventId;
    int MapId;
private:

    // int PathId; // TaxiPath Id
    // int Flags;

};

class TaxiPath
{
public:
    TaxiPath(DBCFile::Iterator data, Noggit::NoggitRenderContext context);
    TaxiPath(unsigned int path_id, Noggit::NoggitRenderContext context);

    void drawPathNodes(glm::mat4x4 mvp, glm::vec3 camera_pos, float cull_distance);
    std::vector<TaxiPathNode> PathNodes;
    //TaxiPath(const TaxiPath&) = delete;

    bool selected;
    int Id;

private:
    int starting_node_id;
    int destination_node_id;
    int Cost;
    // Noggit::Rendering::Primitives::Sphere _sphere_render;
};

enum TaxiNodeType
{
    Taxi,
    Transport
};


class TaxiNode : GenericSelectableObject
{
public:
    TaxiNode(DBCFile::Iterator data, Noggit::NoggitRenderContext context);

    TaxiNodeType taxiNodeType;

    // glm::vec3 Position;
    std::vector<TaxiPath> taxiPaths;

    void draw(glm::mat4x4 mvp, glm::vec3 camera_pos, float cull_distance);
    //TaxiNode(const TaxiNode&) = delete;

    void recalcExtents() override;
    void ensureExtents() override;
    virtual void updateDetails(Noggit::Ui::detail_infos* detail_widget) override;

    glm::vec3 position() { return pos; };

private:
    int Id;
    int MapId;
    std::string Name;
    int CreatureMountIdAlliance;
    int CreatureMountIdHorde;

    // Noggit::Rendering::Primitives::Sphere _sphere_render;
};

class Taxis
{
public:

    explicit Taxis(unsigned int mapid, Noggit::NoggitRenderContext context);

    std::vector<TaxiNode> taxiNodes;
    // std::vector<TaxiPath> taxiPaths;

private:

    Noggit::NoggitRenderContext _context;

    // Noggit::Rendering::Primitives::Sphere _sphere_render;
};

namespace Noggit
{
    namespace Ui
    {
        class TaxiPathEditor : public QWidget
        {
            Q_OBJECT

        public:
            TaxiPathEditor(MapView* map_view, QWidget* parent = nullptr);

        private:

        };

        class TaxiPathNodeEditor : public QWidget
        {
            Q_OBJECT

        public:
            TaxiPathNodeEditor(MapView* map_view, QWidget* parent = nullptr);

            void LoadPathNode(TaxiPathNode* path_node);
        private:
            MapView* _map_view;
            TaxiPathNode* _curr_path_node;

            QSpinBox* _id_spinbox;
            QSpinBox* _id_node_spinbox;
            QSpinBox* _delay_spinbox;
            QSpinBox* _script_arrival_spinbox;
            QSpinBox* _script_departure_spinbox;
        };

        class TaxiEditor : public QWidget
        {
            Q_OBJECT

        public:
            TaxiEditor(MapView* map_view, QWidget* parent = nullptr);

            // TaxiPath* selected_path;
            int selected_node_id = 0;
            int selected_path_id = 0;

        // signals:
        //     void selected(int node_id);
        void taxi_path_node_selected(TaxiPathNode* path_node);

        private:
            QTreeWidget* _taxi_nodes_tree;
            std::map<int, QTreeWidgetItem*> _items;

            TaxiPathNodeEditor* _path_node_editor_widget;

            void buildTaxiNodesList();

            QTreeWidgetItem* add_taxi_node_item(int node_id);

            QTreeWidgetItem* create_or_get_tree_widget_item(int node_id);

            QComboBox* _node_filter_type;

            QComboBox* _path_filter_direction;

            int mapID;

            MapView* _map_view;

            TaxiPath* current_path;
            TaxiNode* current_node;

            TaxiPath* set_current_path(int path_id);

        };



    }
}
