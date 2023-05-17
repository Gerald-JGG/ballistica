// Released under the MIT License. See LICENSE for details.

#ifndef BALLISTICA_BASE_GRAPHICS_MESH_MESH_BUFFER_VERTEX_SPRITE_H_
#define BALLISTICA_BASE_GRAPHICS_MESH_MESH_BUFFER_VERTEX_SPRITE_H_

#include "ballistica/base/graphics/mesh/mesh_buffer.h"

namespace ballistica::base {

// just make this a vanilla child class of our template
// (simply so we could predeclare this)
class MeshBufferVertexSprite : public MeshBuffer<VertexSprite> {
  using MeshBuffer::MeshBuffer;
};

}  // namespace ballistica::base

#endif  // BALLISTICA_BASE_GRAPHICS_MESH_MESH_BUFFER_VERTEX_SPRITE_H_