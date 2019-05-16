#include <io/cdrom_disk.hpp>

#include <util/fs.hpp>
#include <util/log.hpp>

#include <algorithm>
#include <array>

namespace io {

void CdromDisk::init_from_bin(const std::string& bin_path) {
  filepath = bin_path;
  create_track_for_bin(bin_path);

  m_tracks[0].file.open(bin_path, std::ios::binary | std::ios::in);
}

void CdromDisk::init_from_cue(const std::string& cue_path) {
  std::ifstream cue_file(cue_path, std::ios::in);

  // TODO: Cue parsing
}

buffer CdromDisk::read(CdromPosition pos) {
  auto track = get_track_by_pos(pos);

  if (!track) {
    LOG_WARN("Reading failed, no disk loaded");
    return {};
  }

  buffer sector_buf(SECTOR_SIZE);

  // Convert physical position (as on real CDROMs) to logical (as on .BIN files)
  if (track->number == 1 && track->type == CdromTrack::DataType::Data)
    pos.physical_to_logical();

  const auto seek_pos = pos.to_lba() * SECTOR_SIZE;
  track->file.seekg(seek_pos);
  track->file.read((char*)sector_buf.data(), SECTOR_SIZE);

  const std::array<u8, 12> SYNC_MAGIC = { { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                            0xff, 0x00 } };
  auto sync_match = std::equal(SYNC_MAGIC.begin(), SYNC_MAGIC.end(), sector_buf.begin());

  if (track->type == CdromTrack::DataType::Data && !sync_match)
    LOG_ERROR("Invalid sync data in read Data sector");
  else if (track->type == CdromTrack::DataType::Audio && sync_match)
    LOG_ERROR("Sync data found in Data sector");

  return sector_buf;
}

void CdromDisk::create_track_for_bin(const std::string& bin_path) {
  CdromTrack bin_track{};

  const auto filesize = fs::file_size(bin_path);
  if (filesize == 0)
    return;

  bin_track.filepath = bin_path;
  bin_track.number = 1;  // Track number 01
  bin_track.type = CdromTrack::DataType::Data;
  bin_track.frame_count = static_cast<u32>(filesize) / SECTOR_SIZE;
  // the rest of the fields are correct as default-initialized

  m_tracks.clear();
  m_tracks.emplace_back(std::move(bin_track));
}

}  // namespace io