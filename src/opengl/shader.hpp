// This file is part of Noggit3, licensed under GNU General Public License (version 3).

#pragma once

#include <opengl/texture.hpp>

#include <external/tsl/robin_map.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <initializer_list>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <array>
#include <optional>

#include <QObject>
#include <QString>

class QFileSystemWatcher;
class QTimer;

namespace math
{
  struct matrix_4x4;
  struct vector_2d;
  struct vector_3d;
}

namespace OpenGL
{
  namespace Scoped
  {
    struct use_program;
  }

  struct shader
  {
    shader(GLenum type, std::string const& source);
    ~shader();

    static std::string src_from_qrc(std::string const& shader_alias);
    static std::string src_from_qrc(std::string const& shader_alias, std::vector<std::string> const& defines);

    // Issue #63 (shader hot reloading for development): reads the shader
    // from the disk deploy directory (<exe>/shaders or $NOGGIT_SHADER_DIR)
    // when disk shader mode is enabled, falling back to the QRC bundle
    // otherwise. The QRC resources therefore stay the shipping default.
    static std::string src_from_file_or_qrc(std::string const& shader_alias);
    static std::string src_from_file_or_qrc(std::string const& shader_alias, std::vector<std::string> const& defines);

    // whether shaders are read from disk (enabled via the
    // developer/shader_hot_reload setting or the NOGGIT_SHADER_DIR
    // environment variable) and a shader directory actually exists.
    static bool use_disk_shaders();
    // resolved on-disk shader directory, empty when unavailable/disabled
    static QString shader_directory();
    // maps a shader alias (e.g. "m2_vs") to its disk file name ("m2_vert.glsl")
    static QString file_name_for_alias(std::string const& shader_alias);

    shader (shader const&) = delete;
    shader (shader&&) = delete;
    shader& operator= (shader const&) = delete;
    shader& operator= (shader&&) = delete;

  private:
    friend struct program;

    GLuint _handle;
  };

  // Issue #63: watches the on-disk shader directory and notifies every
  // registered component when a shader file changes (debounced). Callbacks
  // run without a current GL context and may come from another thread, so
  // a callback should only flag the change and let the component's regular
  // draw method rebuild its GL state in its owning context (VAOs are not
  // shared across GL contexts). Rebuilds are expected to keep the previous
  // GL state alive when the new sources fail to compile.
  class shader_reloader : public QObject
  {
    Q_OBJECT

  public:
    static shader_reloader* instance();

    // Registers a callback notified (debounced) whenever a shader file
    // changes. Returns a registration id used by remove_reload_callback().
    // Registration also (lazily) starts the watcher when disk shader mode
    // is enabled.
    int add_reload_callback(std::function<void()> callback);
    void remove_reload_callback(int id);

    bool isWatching() const;

  private:
    shader_reloader();

    void start_watching();
    void reload_all();

    QFileSystemWatcher* _watcher = nullptr;
    QTimer* _debounce_timer = nullptr;
    std::map<int, std::function<void()>> _callbacks;
    int _next_id = 0;
  };

  struct program
  {
    program (std::initializer_list<shader>);
    ~program();

    program (program const&) = delete;
    program (program&&);
    program& operator= (program const&) = delete;
    program& operator= (program&&) = delete;

    tsl::robin_map<std::string, GLuint> const* getUniforms() const { return &_uniforms; };
    tsl::robin_map<std::string, GLuint> const* getAttributes() const { return &_attribs; };
    tsl::robin_map<GLuint, GLint> const* getUniformsIntCache() const { return &_uniforms_int_cache; };
    tsl::robin_map<GLuint, GLfloat> const* getUniformsFloatCache() const { return &_uniforms_float_cache; };
    tsl::robin_map<GLuint, bool> const* getUniformsBoolCache() const { return &_uniforms_bool_cache; };

  private:
    GLuint uniform_location (std::string const& name) const;
    GLuint uniform_block_location (std::string const& name) const;
    GLuint attrib_location (std::string const& name) const;

    friend struct Scoped::use_program;

    std::optional<GLuint> _handle;

    tsl::robin_map<std::string, GLuint> _uniforms;
    tsl::robin_map<std::string, GLuint> _attribs;

    tsl::robin_map<GLuint, GLint> _uniforms_int_cache;
    tsl::robin_map<GLuint, GLfloat> _uniforms_float_cache;
    tsl::robin_map<GLuint, bool> _uniforms_bool_cache;
  };

  namespace Scoped
  {
    struct use_program
    {
      use_program (program const&);
      ~use_program();

      use_program (use_program const&) = delete;
      use_program (use_program&&) = delete;
      use_program& operator= (use_program const&) = delete;
      use_program& operator= (use_program&&) = delete;

      void bind_uniform_block(std::string const& name, unsigned);
      void uniform (std::string const& name, std::vector<int> const&);
      void uniform (std::string const& name, int const* data, std::size_t size);
      void uniform (std::string const& name, glm::vec3 const* data, std::size_t size);
      void uniform (GLint pos, std::vector<int> const&);
      void uniform (std::string const& name, GLint);
      void uniform (GLint pos, GLint);
      void uniform (std::string const& name, GLfloat);
      void uniform (GLint pos, GLfloat);
      void uniform (std::string const& name, bool);
      void uniform (GLint pos, bool);
      void uniform (std::string const& name, std::vector<glm::vec3> const& value);
      void uniform(std::string const& name, std::array<glm::vec3, 8> const& value);
      void uniform (GLint pos, std::vector<glm::vec3> const& value);
      void uniform(std::string const& name, std::vector<glm::vec4> const& value);
      void uniform(GLint pos, std::vector<glm::vec4> const& value);
      void uniform (std::string const& name, glm::vec2 const&);
      void uniform (GLint pos, glm::vec2 const&);
      void uniform (std::string const& name, glm::vec3 const&);
      void uniform (GLint pos, glm::vec3 const&);
      void uniform (std::string const& name, glm::vec4 const&);
      void uniform (GLint pos, glm::vec4 const&);
      void uniform(std::string const& name, glm::mat4x4 const&);
      void uniform(GLint pos, glm::mat4x4 const&);
      template<typename T> void uniform (std::string const&, T) = delete;

      void uniform_chunk_textures (std::string const& name, std::array<std::array<std::array<int, 2>, 4>, 256> const& value);

      void uniform_cached (std::string const& name, GLint);
      void uniform_cached (std::string const& name, GLfloat);
      void uniform_cached (std::string const& name, bool);

      void sampler (std::string const& name, GLenum texture_slot, texture*);

      void attrib (std::string const& name, std::vector<float> const&);
      void attrib (std::string const& name, std::vector<glm::vec2> const&);
      void attrib (std::string const& name, std::vector<glm::vec3> const&);
      void attrib (std::string const& name, glm::vec3 const*);
      void attrib (std::string const& name, glm::mat4x4 const*, GLuint divisor = 0);
      void attrib (std::string const& name, GLsizei size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* data);
      void attrib (std::string const& name, GLuint buffer, GLsizei size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* data);
      void attribi (std::string const& name, GLuint buffer, GLsizei size, GLenum type, GLsizei stride, const GLvoid* data);
      void attribi (std::string const& name, GLsizei size, GLenum type, GLsizei stride, const GLvoid* data);

      void attrib_divisor(std::string const& name, GLuint divisor, GLsizei range = 1);

    private:
      GLuint uniform_location (std::string const& name);
      GLuint uniform_block_location (std::string const& name);
      GLuint attrib_location (std::string const& name);

      program const& _program;
      std::set<GLuint> _enabled_vertex_attrib_arrays;

      GLuint _old;
    };
  }
}
