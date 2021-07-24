<h1 align="center">mini core</h1>
<blockquote>A minimal header-only C++ OpenGL 3d game framework</blockquote>

Minimal means minimal! The only thing this framework can do is render things. This means that you need to load models, textures, shaders and other things yourself.

The framework providers a deferred renderer with support for simple directional lighting and textures (shadows, you're next!).
> Note: there is a forward renderer with minimal support for lighting and I don't really plan to support it (maybe after shadows are done?)

<h3 align="center">Documentation + File Structure</h3>

The file structure is a little weird: <small>[todo: doxygen!!]</small>
* `core.hpp` the actual framework
  * depends on:
    * [`glm`](https://github.com/g-truc/glm/) <small>[todo: submodule!]</small>
    * [`glfw`](https://www.glfw.org/)
    * [`glad`](https://glad.dav1d.de/) <small>my old mac only supports gl 4.1, so that</small>
  * namespace `srd::core` <small>(srd ↝ somerandomdev)</small>
    * namespace `window`
      * struct `window` <small>[change name to be different from the ns?]</small>
        * `int width, height`
        * `const char *title`
    * namespace `math`
      * struct `transform`
        * `glm::mat4 matrix` - the transformation matrix
        * `glm::vec3 position` - the position vector
        * `glm::quat rotation` - the rotation quaternion
        * `glm::vec3 scale` - the scale vector
        * `void update()` - updates the `matrix` to reflect the changes in other fields
    * namespace `gfx`
      * struct `shader`
        * `unsigned int id` - the id of the shader program
        * `void use() const` - uses the opengl program
        * `void setUniform(int location, XXX value) const` - sets a uniform at `location` to `value`
        * `int getUniform(const std::string &name) const` - returns a location of a uniform with a name.
        * `shader(const std::string &vertex, const std::string &fragment)` - creates a shader with a vertex shader source and a fragment shader source.
        * `~shader()` - deletes the shader program.
    * struct `mesh` - TODO
    * struct `gbuffer` - TODO <small>note: set width and height to window's w and h</small>
    * struct `deferred_renderer` - TODO <small>note: same as gbuffer</small>
    
* `main.cpp` - an example/tester that uses `srd::core`
* `util.hpp` - what `main.cpp` uses to load models and textures
  * depends on:
    * [`tiny_obj_loader.h`](https://github.com/tinyobjloader/tinyobjloader/)
    * [`stb_image.h`](https://github.com/nothings/stb/blob/master/stb_image.h)
* `log.hpp` - a tiny logging library, only needed for `main.cpp` and `log.hpp`
* `log.cpp` - `log.hpp` default imlementation <small>{take a look}</small>
* `runfile` - similar to `Makefile` for my `make` alternative (just run the console command)

The header file uses the same guard as most of `stb`'s headers do:
```cpp
// Somewhere in a .cpp file, once
#define SRD_CORE_IMPLEMENTATION
#include <.../.../core.hpp>
```
**Important!** `srd::core::window::show(...)` is defined only when `SRD_CORE_IMPLEMENTATION` is defined! <small>(this is because the function needs to have templates for lambdas! [`std::function` is slow...])</small>

<h3 align="center">Example</h3>

**TODO!** See `main.cpp`!!! <small>(it's small)</small>

<h3 align="center">Notes</h3>

Started working on *`July, 20th 2021`*.
