#pragma once

#include <util/log.hpp>
#include <util/types.hpp>

#include <array>

namespace memory {
class Ram;
}

namespace gpu {
class Gpu;
}

namespace memory {

enum class DmaPort {
  MdecIn = 0,   // Macroblock decoder input
  MdecOut = 1,  // Macroblock decoder output
  Gpu = 2,      // Graphics Processing Unit
  CdRom = 3,    // CD-ROM drive
  Spu = 4,      // Sound Processing Unit
  Pio = 5,      // Extension port
  Otc = 6,      // Used to clear the ordering table
};
static const char* dma_port_to_str(DmaPort dma_port) {
  switch (dma_port) {
    case DmaPort::MdecIn: return "MDECin";
    case DmaPort::MdecOut: return "MDECout";
    case DmaPort::Gpu: return "GPU";
    case DmaPort::CdRom: return "CD-ROM";
    case DmaPort::Spu: return "SPU";
    case DmaPort::Pio: return "PIO";
    case DmaPort::Otc: return "OTC";
    default: return "<Invalid>";
  }
}

union DmaChannelControl {
  u32 word{};

  struct {
    u32 transfer_direction : 1;
    u32 memory_address_step : 1;
    u32 _2_7 : 6;
    u32 chopping_enable : 1;
    u32 sync_mode : 2;
    u32 _11_15 : 5;
    u32 chopping_dma_window_size : 3;
    u32 _19 : 1;
    u32 chopping_cpu_window_size : 3;
    u32 _23 : 1;
    u32 enable : 1;
    u32 _25_27 : 3;
    u32 manual_trigger : 1;
    u32 _29_31 : 3;
  };
};

union DmaBlockControl {  // Fields are different depending on sync mode
  u32 word{};

  union {
    // Manual sync mode
    struct {
      u16 word_count;  // number of words to transfer
    } manual;
    // Request sync mode
    struct {
      u16 block_size;   // block size in words
      u16 block_count;  // number of blocks to transfer
    } request;
    // In Linked List mode this register is unused
  };
};

class DmaChannel {
 public:
  enum class TransferDirection {
    ToRam = 0,
    FromRam = 1,
  };
  enum class MemoryAddressStep {
    Forward = 0,
    Backward = 1,
  };
  enum class SyncMode {
    // Transfer starts when CPU writes to the manual_trigger bit and happens all at once
    // Used for CDROM, OTC
    Manual = 0,
    // Sync blocks to DMA requests
    // Used for MDEC, SPU, and GPU-data
    Request = 1,
    // Used for GPU-command-lists
    LinkedList = 2,
  };

  bool active() const;
  u32 transfer_word_count();
  void transfer_finished();
  TransferDirection transfer_direction() const {
    return static_cast<TransferDirection>(m_channel_control.transfer_direction);
  }
  bool to_ram() const { return transfer_direction() == TransferDirection::ToRam; }
  MemoryAddressStep memory_address_step() const {
    return static_cast<MemoryAddressStep>(m_channel_control.memory_address_step);
  }
  SyncMode sync_mode() const { return static_cast<SyncMode>(m_channel_control.sync_mode); }
  const char* sync_mode_str() const {
    switch (sync_mode()) {
      case SyncMode::Manual: return "Manual";
      case SyncMode::Request: return "Request";
      case SyncMode::LinkedList: return "Linked List";
      default: return "<Invalid>";
    }
  }

  DmaChannelControl m_channel_control;
  DmaBlockControl m_block_control;
  u32 m_base_addr{};
};

enum class DmaRegister : u32 {
  DMA_GPU_CONTROL = 0x28,
  DMA_CONTROL = 0x70,
  DMA_INTERRUPT = 0x74,
};

union DmaInterruptRegister {
  u32 word{};

  struct {
    u32 _0_14 : 15;
    u32 force : 1;

    u32 dec_in_enable : 1;
    u32 dec_out_enable : 1;
    u32 gpu_enable : 1;
    u32 cdrom_enable : 1;
    u32 spu_enable : 1;
    u32 ext_enable : 1;
    u32 ram_enable : 1;
    u32 master_enable : 1;

    u32 dec_in_flags : 1;
    u32 dec_out_flags : 1;
    u32 gpu_flags : 1;
    u32 cdrom_flags : 1;
    u32 spu_flags : 1;
    u32 ext_flags : 1;
    u32 ram_flags : 1;
    u32 master_flags : 1;
  };
};

class Dma {
 public:
  explicit Dma(memory::Ram& ram, gpu::Gpu& gpu) : m_ram(ram), m_gpu(gpu) {}

  void set_reg(DmaRegister reg, u32 val);
  u32 read_reg(DmaRegister reg) const;
  DmaChannel const& channel_control(DmaPort port) const;
  DmaChannel& channel_control(DmaPort port);

 private:
  void do_transfer(DmaPort port);
  void do_block_transfer(DmaPort port);
  void do_linked_list_transfer(DmaPort port);

 private:
  DmaInterruptRegister m_interrupt;
  std::array<DmaChannel, 7> m_channels{};
  u32 m_control{ 0x07654321 };

  memory::Ram& m_ram;
  gpu::Gpu& m_gpu;
};

}  // namespace memory
