
/* This structure is a carbon copy of OpenSubDiv's PatchCoord. */
struct BlenderPatchCoord {
  int patch_index;
  uint encoded_uv;
};

vec2 decode_uv(uint encoded_uv)
{
  float u = float((encoded_uv >> 16) & 0xffff) / 65535.0;
  float v = float((encoded_uv) & 0xffff) / 65535.0;
  return vec2(u, v);
}

/* This structure is a carbon copy of OpenSubDiv's PatchTable::PatchHandle. */
struct PatchHandle {
  int array_index;
  int patch_index;
  int vertex_index;
};

/* This structure is a carbon copy of OpenSubDiv's PatchCoord. */
struct PatchCoord {
  int array_index;
  int patch_index;
  int vertex_index;
  float u;
  float v;
};

/* This structure is a carbon copy of OpenSubDiv's PatchCoord.QuadNode.
 * Each child is a bitfield. */
struct QuadNode {
  uvec4 child;
};

bool is_set(uint i)
{
  /* QuadNode.Child.isSet is the first bit of the bitfield. */
  return (i & 0x1) != 0;
}

bool is_leaf(uint i)
{
  /* QuadNode.Child.isLeaf is the second bit of the bitfield. */
  return (i & 0x2) != 0;
}

uint get_index(uint i)
{
  /* QuadNode.Child.index is made of the remaining bits. */
  return (i >> 2) & 0x3FFFFFFF;
}

/* Duplicate of #PosNorLoop from the mesh extract CPU code.
 * We do not use a vec3 for the position as it will be padded to a vec4 which is incompatible with
 * the format.  */
struct PosNorLoop {
  float x, y, z;
  uint nor;
};

vec3 get_vertex_pos(PosNorLoop vertex_data)
{
  return vec3(vertex_data.x, vertex_data.y, vertex_data.z);
}

float gpu_unpack_float_from_uint(uint x)
{
  return (float(x) - 512.0) / 511.0;
}

uint gpu_pack_float_from_uint(float x)
{
  return uint(clamp(x * 511.0 + 512.0, 0.0, 1023.0));
}

vec3 get_vertex_nor(PosNorLoop vertex_data)
{
  uint inor = vertex_data.nor;
  float x = gpu_unpack_float_from_uint(inor & 0x3ff);
  float y = gpu_unpack_float_from_uint((inor >> 10) & 0x3ff);
  float z = gpu_unpack_float_from_uint((inor >> 20) & 0x3ff);
  return vec3(x, y, z);
}

void set_vertex_pos(inout PosNorLoop vertex_data, vec3 pos)
{
  vertex_data.x = pos.x;
  vertex_data.y = pos.y;
  vertex_data.z = pos.z;
}

/* Set the vertex normal but preserve the existing flag. This is for when we compute manually the
 * vertex normals when we cannot use the limit surface, in which case the flag and the normal are
 * set by two separate compute pass. */
void set_vertex_nor(inout PosNorLoop vertex_data, vec3 nor)
{
  uint x = gpu_pack_float_from_uint(nor.x);
  uint y = gpu_pack_float_from_uint(nor.y);
  uint z = gpu_pack_float_from_uint(nor.z);
  uint flag = (vertex_data.nor >> 30) & 0x3;
  uint inor = x | y << 10 | z << 20 | flag << 30;
  vertex_data.nor = inor;
}

void set_vertex_nor(inout PosNorLoop vertex_data, vec3 nor, uint flag)
{
  uint x = gpu_pack_float_from_uint(nor.x);
  uint y = gpu_pack_float_from_uint(nor.y);
  uint z = gpu_pack_float_from_uint(nor.z);
  uint inor = x | y << 10 | z << 20 | flag << 30;
  vertex_data.nor = inor;
}

#define ORIGINDEX_NONE -1