#include <bios.hpp>
#include <fs.hpp>
#include <load_file.hpp>

#include <memory>

namespace bios {

Bios::Bios(fs::path const& path) {
  auto buf = util::load_file(path);

  m_data = std::make_unique<std::array<byte, BIOS_SIZE>>();

  std::copy(buf.begin(), buf.end(), m_data->begin());
}

u32 Bios::read32(u32 offset) const {
  return *(u32*)(m_data.get()->data() + offset);
}

}  // namespace bios
