// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#pragma once

#include <opengl/scoped.hpp>
#include <opengl/shader.hpp>
#include <math/trig.hpp>
#include <noggit/ContextObject.hpp>

#include <atomic>
#include <memory>

namespace math
{
  struct vector_3d;
  struct vector_4d;
}

class World;
namespace Noggit::Rendering::Primitives
{
  class WireBox
  {
  public:
    WireBox() = default;
    WireBox(const WireBox&) = delete;
    WireBox& operator=(WireBox& box );

  public:
    static WireBox& getInstance(Noggit::NoggitRenderContext context);

    void draw ( glm::mat4x4 const& model_view
              , glm::mat4x4 const& projection
              , glm::mat4x4 const& transform
              , glm::vec4 const& color
              , glm::vec3 const& min_point
              , glm::vec3 const& max_point
              );

    void unload();

  private:
    bool _buffers_are_setup = false;

    // Issue #63 (shader hot reload): id of the registered reload callback
    // or -1 when not registered, and a flag set by that callback so the
    // next draw rebuilds the GL state in its own context.
    int _shader_reload_registration = -1;
    std::atomic<bool> _shader_reload_dirty{false};

    void setup_buffers();
    // rebuilds the GL state from the current on-disk shader sources,
    // keeping the previous state when they fail to compile
    void reload_program();

    OpenGL::Scoped::deferred_upload_vertex_arrays<1> _vao;
    OpenGL::Scoped::deferred_upload_buffers<1> _buffers;
    GLuint const& _indices = _buffers[0];
    std::unique_ptr<OpenGL::program> _program;
  };

  class Grid
  {
  public:
      void draw(glm::mat4x4 const& mvp
          , glm::vec3 const& pos
          , glm::vec4  const& color
          , float radius
      );
      void unload();
  private:
      bool _buffers_are_setup = false;

      // Issue #63 (shader hot reload): see WireBox.
      int _shader_reload_registration = -1;
      std::atomic<bool> _shader_reload_dirty{false};

      void setup_buffers();
      void reload_program();

      int _indice_count = 0;

      OpenGL::Scoped::deferred_upload_vertex_arrays<1> _vao;
      OpenGL::Scoped::deferred_upload_buffers<2> _buffers;
      GLuint const& _vertices_vbo = _buffers[0];
      GLuint const& _indices_vbo = _buffers[1];
      std::unique_ptr<OpenGL::program> _program;
  };

  class Sphere
  {
  public:
      void draw(glm::mat4x4 const& mvp
          , glm::vec3 const& pos
          , glm::vec4  const& color
          , float radius
          , int longitude = 32
          , int latitude = 18
          , float alpha = 1.f
          , bool wireframe = false
          , bool both = false
             );
    void unload();

  private:
    bool _buffers_are_setup = false;

    // Issue #63 (shader hot reload): see WireBox. Note that the reload
    // keeps the longitude/latitude of the current geometry.
    int _shader_reload_registration = -1;
    std::atomic<bool> _shader_reload_dirty{false};

    void setup_buffers(int longitude, int latitude);
    void reload_program();

    int _indice_count = 0;
    int _longitude = 32;
    int _latitude = 18;

    OpenGL::Scoped::deferred_upload_vertex_arrays<1> _vao;
    OpenGL::Scoped::deferred_upload_buffers<2> _buffers;
    GLuint const& _vertices_vbo = _buffers[0];
    GLuint const& _indices_vbo = _buffers[1];
    std::unique_ptr<OpenGL::program> _program;
  };

  class Square
  {
  public:
    void draw(glm::mat4x4 const& mvp
             , glm::vec3 const& pos
             , float radius // radius of the biggest circle fitting inside the square drawn
             , math::radians inclination
             , math::radians orientation
             , glm::vec4  const& color
             );
    void unload();
  private:
    bool _buffers_are_setup = false;

    // Issue #63 (shader hot reload): see WireBox.
    int _shader_reload_registration = -1;
    std::atomic<bool> _shader_reload_dirty{false};

    void setup_buffers();
    void reload_program();

    OpenGL::Scoped::deferred_upload_vertex_arrays<1> _vao;
    OpenGL::Scoped::deferred_upload_buffers<2> _buffers;
    GLuint const& _vertices_vbo = _buffers[0];
    GLuint const& _indices_vbo = _buffers[1];
    std::unique_ptr<OpenGL::program> _program;
  };

  /*class Cylinder
  {
  public:
      void draw(glm::mat4x4 const& mvp, glm::vec3 const& pos, const glm::vec4 color, float radius, int precision, World* world, int height = 10);
      void unload();

  private:
      bool _buffers_are_setup = false;

      // Issue #63 (shader hot reload): id of the registered reload callback
      // or -1 when not registered.
      int _shader_reload_registration = -1;
      void setup_buffers(int precision, World* world, int height);
      int _indice_count = 0;

      OpenGL::Scoped::deferred_upload_vertex_arrays<1> _vao;
      OpenGL::Scoped::deferred_upload_buffers<2> _buffers;
      GLuint const& _vertices_vbo = _buffers[0];
      GLuint const& _indices_vbo = _buffers[1];
      std::unique_ptr<OpenGL::program> _program;
  };*/

  class Line
  {
  public:
      void initSpline();
      void draw(glm::mat4x4 const& mvp, std::vector<glm::vec3> const& points, glm::vec4 const& color, bool spline);
      void unload();

  private:
      bool _buffers_are_setup = false;

      // Issue #63 (shader hot reload): Line does not register with the
      // shader_reloader because it rebuilds its program on every draw
      // anyway; its draw() only guards against failing disk shaders.
      void setup_buffers(std::vector<glm::vec3> const points);

      void setup_buffers_interpolated(std::vector<glm::vec3> const points);
      glm::vec3 interpolate(float t, glm::vec3 p0, glm::vec3 p1, glm::vec3 m0, glm::vec3 m1);

      int _indice_count = 0;

      void setup_shader(std::vector<glm::vec3> vertices, std::vector<std::uint16_t> indices);
      OpenGL::Scoped::deferred_upload_vertex_arrays<1> _vao;
      OpenGL::Scoped::deferred_upload_buffers<2> _buffers;
      GLuint const& _vertices_vbo = _buffers[0];
      GLuint const& _indices_vbo = _buffers[1];
      std::unique_ptr<OpenGL::program> _program;
  };

}
