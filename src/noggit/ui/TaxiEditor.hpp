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


class TaxiPathNode : SceneObject
{
public:
    TaxiPathNode(DBCFile::Iterator data, Noggit::NoggitRenderContext context);

    TaxiPathNode(TaxiPathNode&& other)
        : SceneObject(other._type, other._context)
    {
        std::swap(extents, other.extents);
        pos = other.pos;
        dir = other.dir;
        _context = other._context;
        uid = other.uid;

        _transform_mat = other._transform_mat;
        _transform_mat_inverted = other._transform_mat_inverted;
    }

    void intersect(math::ray const& ray, selection_result* results);
    // glm::vec3 Position;

    void draw(glm::mat4x4 mvp, glm::vec3 camera_pos, float cull_distance);

    glm::vec3 position() { return pos; };

    // SceneObject stuff, a lot of it is not needed
    void recalcExtents() override;
    void ensureExtents() override;
    bool finishedLoading() override { return true; };
    virtual void updateDetails(Noggit::Ui::detail_infos* detail_widget) override;

    [[nodiscard]]
    AsyncObject* instance_model() const override { return nullptr; };
private:
    int Id;
    // int PathId; // TaxiPath Id
    int NodeIndex;
    int MapId;

    // int Flags;
    int Delay;
    int ArrivalEventId;
    int DepartureEventId;
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


class TaxiNode : SceneObject
{
public:
    TaxiNode(DBCFile::Iterator data, Noggit::NoggitRenderContext context);

    TaxiNodeType taxiNodeType;

    // glm::vec3 Position;
    std::vector<TaxiPath> taxiPaths;

    void draw(glm::mat4x4 mvp, glm::vec3 camera_pos, float cull_distance);
    //TaxiNode(const TaxiNode&) = delete;

    // SceneObject stuff, a lot of it is not needed
    void recalcExtents() override;
    void ensureExtents() override;
    bool finishedLoading() override { return true; };
    virtual void updateDetails(Noggit::Ui::detail_infos* detail_widget) override;

    [[nodiscard]]
    AsyncObject* instance_model() const override { return nullptr; };

    glm::vec3 position() { return pos; };

private:
    int Id;
    int MapId;
    std::string Name;
    int CreatureMountIdAlliance;
    int CreatureMountIdHorde;

    std::unique_ptr<OpenGL::program> _program;
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
        class TaxiEditor : public QWidget
        {
            Q_OBJECT

        public:
            TaxiEditor(MapView* map_view, QWidget* parent = nullptr);

            // TaxiPath* selected_path;
            int selected_node_id = 0;
            int selected_path_id = 0;

        signals:
            void selected(int node_id);

        private:
            QTreeWidget* _taxi_nodes_tree;
            std::map<int, QTreeWidgetItem*> _items;

            void buildTaxiNodesList();

            QTreeWidgetItem* add_taxi_node_item(int node_id);

            QTreeWidgetItem* create_or_get_tree_widget_item(int node_id);

            int mapID;

            MapView* _map_view;

            TaxiPath* current_path;
            TaxiNode* current_node;

            TaxiPath* set_current_path(int path_id);
        };
    }
}
