#pragma once

#include <algorithm>
#include <array>
#include <gpu/colors.hpp>
#include <renderer/buffer.hpp>
#include <util/bit_utils.hpp>
#include <util/log.hpp>
#include <util/types.hpp>

namespace gpu {
class Gpu;
}

namespace renderer {
namespace rasterizer {

constexpr u32 MAX_GP0_CMD_LEN = 32;

enum class QuadTriangleIndex {
  None,
  First,
  Second,
};

struct Color;
struct Position;
struct Texcoord;

using Color3 = std::array<Color, 3>;
using Color4 = std::array<Color, 4>;
using Position3 = std::array<Position, 3>;
using Position4 = std::array<Position, 4>;
using Texcoord3 = std::array<Texcoord, 3>;
using Texcoord4 = std::array<Texcoord, 4>;

struct Position {
  s16 x;
  s16 y;

  static Position from_gp0(u32 cmd) {
    return { bit_utils::sign_extend<10, s16>((s16)cmd & 0x7FF),
             bit_utils::sign_extend<10, s16>((s16)(cmd >> 16) & 0x7FF) };
  }
  static Position from_gp0_fill(u32 cmd) {
    return { static_cast<s16>(cmd & 0x3F0), static_cast<s16>((cmd >> 16) & 0x1FF) };
  }
  static Position3 from_gp0(u32 cmd, u32 cmd2, u32 cmd3) {
    return { from_gp0(cmd), from_gp0(cmd2), from_gp0(cmd3) };
  }
  static Position4 from_gp0(u32 cmd, u32 cmd2, u32 cmd3, u32 cmd4) {
    return { from_gp0(cmd), from_gp0(cmd2), from_gp0(cmd3), from_gp0(cmd4) };
  }
  Position operator+(const Position& rhs) const {
    // TODO: sign extend?
    return { static_cast<s16>(x + rhs.x), static_cast<s16>(y + rhs.y) };
  }
};

struct Size {
  s16 width;
  s16 height;

  static Size from_gp0(u32 cmd) {
    return { bit_utils::sign_extend<10, s16>((s16)cmd & 0x1FF),
             bit_utils::sign_extend<10, s16>((s16)(cmd >> 16) & 0x3FF) };
  }
  static Size from_gp0_fill(u32 cmd) {
    return { static_cast<s16>((static_cast<s16>(cmd & 0x3FF) + 0x0f) & ~0x0f),
             static_cast<s16>((cmd >> 16) & 0x1ff) };
  }
};

struct Color {
  u8 r;
  u8 g;
  u8 b;

  static Color from_gp0(u32 cmd) {
    return { static_cast<u8>(cmd), static_cast<u8>(cmd >> 8), static_cast<u8>(cmd >> 16) };
  }
  static Color3 from_gp0(u32 cmd, u32 cmd2, u32 cmd3) {
    return { from_gp0(cmd), from_gp0(cmd2), from_gp0(cmd3) };
  }
  static Color4 from_gp0(u32 cmd, u32 cmd2, u32 cmd3, u32 cmd4) {
    return { from_gp0(cmd), from_gp0(cmd2), from_gp0(cmd3), from_gp0(cmd4) };
  }

  u32 word() const { return r | g << 8 | b << 16; }
};

struct Texcoord {
  s16 x;
  s16 y;

  static Texcoord from_gp0(u32 cmd) { return { (u8)(cmd & 0xFF), (u8)((cmd >> 8) & 0xFF) }; }
  static Texcoord4 from_gp0(u32 cmd1, u32 cmd2, u32 cmd3, u32 cmd4) {
    return { from_gp0(cmd1), from_gp0(cmd2), from_gp0(cmd3), from_gp0(cmd4) };
  }
  Texcoord operator+(const Texcoord& rhs) const {
    // TODO: sign extend?
    return { static_cast<s16>(x + rhs.x), static_cast<s16>(y + rhs.y) };
  }
};

union Palette {
  struct {
    u16 _x : 6;
    u16 _y : 9;
    u16 _pad : 1;
  };
  static Palette from_gp0(u32 cmd) {
    Palette p;
    p.word = (cmd >> 16) & 0xFFFF;
    return p;
  }
  u16 x() const { return _x * 16; }
  u16 y() const { return _y; }

  u16 word;
};

struct TextureInfo {
  Texcoord4 uv;
  Texcoord3 uv_active;  // UVs of currently rendering triangle
  Palette palette;
  u16 page;
  Color color;

  void update_active_triangle(QuadTriangleIndex triangle_index) {
    switch (triangle_index) {
      case QuadTriangleIndex::First: uv_active = { uv[0], uv[1], uv[2] }; break;
      case QuadTriangleIndex::Second: uv_active = { uv[1], uv[2], uv[3] }; break;
      case QuadTriangleIndex::None:
        /* fallthrough */
      default:
        LOG_ERROR("Invalid QuadTriangleIndex");
        assert(0);
        break;
    }
  }
};

// Last 3 values map to GPUSTAT.7-8 "Texture Page Colors"
enum class PixelRenderType {
  SHADED,
  TEXTURED_PALETTED_4BIT,
  TEXTURED_PALETTED_8BIT,
  TEXTURED_16BIT,
};

struct BarycentricCoords {
  s32 a;
  s32 b;
  s32 c;
};

struct TexelPos {
  s32 x;
  s32 y;
};

// First byte of GP0 draw commands
union DrawCommand {
  enum class TextureMode : u8 {
    Blended = 0,
    Raw = 1,
  };
  enum class RectSize : u8 {
    SizeVariable = 0,
    Size1x1 = 1,
    Size8x8 = 2,
    Size16x16 = 3,
  };
  enum class VertexCount : u8 {
    Triple = 0,
    Quad = 1,
  };
  enum class LineCount : u8 {
    Single = 0,
    Poly = 1,
  };
  enum class Shading : u8 {
    Flat = 0,
    Gouraud = 1,
  };
  enum class PrimitiveType : u8 {
    Polygon = 1,
    Line = 2,
    Rectangle = 3,
  };

  u8 word{};

  struct Line {
    u8 : 1;
    u8 semi_transparency : 1;
    u8 : 1;
    LineCount line_count : 1;
    Shading shading : 1;
    u8 : 3;

    bool is_poly() const { return line_count == LineCount::Poly; }
    u8 get_arg_count() const {
      if (is_poly())
        return MAX_GP0_CMD_LEN - 1;
      return 2 + (shading == Shading::Gouraud ? 1 : 0);
    }

  } line;

  struct Rectangle {
    TextureMode texture_mode : 1;
    u8 semi_transparency : 1;
    u8 texture_mapping : 1;
    RectSize rect_size : 2;
    u8 : 3;

    bool is_variable_sized() const { return rect_size == RectSize::SizeVariable; }
    Size get_static_size() const {
      switch (rect_size) {
        case RectSize::Size1x1: return { 1, 1 };
        case RectSize::Size8x8: return { 8, 8 };
        case RectSize::Size16x16: return { 16, 16 };
        case RectSize::SizeVariable:
        default: LOG_ERROR("Invalid size"); return { 0, 0 };
      }
    }
    u8 get_arg_count() const {
      u8 arg_count = 1;

      if (is_variable_sized())
        arg_count += 1;

      if (texture_mapping)
        arg_count += 1;
      return arg_count;
    }
  } rectangle;

  struct Polygon {
    TextureMode texture_mode : 1;
    u8 semi_transparency : 1;
    u8 texture_mapping : 1;
    VertexCount vertex_count : 1;
    Shading shading : 1;
    u8 : 3;

    bool is_quad() const { return vertex_count == VertexCount::Quad; }
    u8 get_vertex_count() const { return is_quad() ? 4 : 3; }
    u8 get_arg_count() const {
      u8 arg_count = get_vertex_count();

      if (texture_mapping)
        arg_count *= 2;

      if (shading == Shading::Gouraud)
        arg_count += get_vertex_count() - 1;
      return arg_count;
    }
  } polygon;

  struct Flags {
    TextureMode texture_mode : 1;
    u8 semi_transparency : 1;
    u8 texture_mapped : 1;
    u8 : 1;
    Shading shading : 1;
  } flags;
};

class Rasterizer {
 public:
  explicit Rasterizer(gpu::Gpu& gpu) : m_gpu(gpu) {}

  template <PixelRenderType RenderType>
  void draw_pixel(Position pos,
                  const Color3* col,
                  const TextureInfo* tex_info,
                  BarycentricCoords bar,
                  s32 area,
                  DrawCommand::Flags draw_flags) const;

  template <PixelRenderType RenderType>
  void draw_triangle(Position3 pos,
                     const Color3* col,
                     const TextureInfo* tex_info,
                     DrawCommand::Flags draw_flags);

  void draw_polygon(const DrawCommand::Polygon& polygon);
  void draw_rectangle(const DrawCommand::Rectangle& polygon);

  void extract_draw_data_polygon(const DrawCommand::Polygon& polygon,
                                 const std::vector<u32>& gp0_cmd,
                                 Position4& positions,
                                 Color4& colors,
                                 TextureInfo& tex_info) const;
  void extract_draw_data_rectangle(const DrawCommand::Rectangle& rectangle,
                                   const std::vector<u32>& gp0_cmd,
                                   Position4& positions,
                                   Color4& colors,
                                   TextureInfo& tex_info,
                                   Size& size) const;

 private:
  void draw_polygon_impl(Position4 positions,
                         Color4 colors,
                         TextureInfo tex_info,
                         bool is_quad,
                         DrawCommand::Flags draw_flags);
  void draw_triangle_textured(Position3 tri_positions,
                              const Color3* col,
                              TextureInfo tex_info,
                              DrawCommand::Flags draw_flags,
                              PixelRenderType pixel_render_type);

  TexelPos calculate_texel_pos(BarycentricCoords bar, s32 area, Texcoord3 uv) const;
  static gpu::RGB16 calculate_pixel_shaded(Color3 colors, BarycentricCoords bar);
  gpu::RGB16 calculate_pixel_tex_4bit(TextureInfo tex_info, TexelPos texel_pos) const;
  gpu::RGB16 calculate_pixel_tex_8bit(TextureInfo tex_info, TexelPos texel_pos) const;
  gpu::RGB16 calculate_pixel_tex_16bit(TextureInfo tex_info, TexelPos texel_pos) const;

 private:
  // GPU reference
  gpu::Gpu& m_gpu;
};

}  // namespace rasterizer
}  // namespace renderer