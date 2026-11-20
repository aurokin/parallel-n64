#include "../state.hpp"

#ifdef PARALLEL_INTEGRATION
#include "../rsp_1.1.h"
#include "m64p_plugin.h"
namespace RSP
{
extern RSP_INFO rsp;
extern short MFC0_count[32];
extern int SP_STATUS_TIMEOUT;
} // namespace RSP
#endif

using namespace RSP;

extern "C"
{

#ifdef INTENSE_DEBUG
	void log_rsp_mem_parallel(void);
#endif

	int RSP_MFC0(RSP::CPUState *rsp, unsigned rt, unsigned rd)
	{
		rd &= 15;
		uint32_t res = *rsp->cp0.cr[rd];
		if (rt)
			rsp->sr[rt] = res;

#ifdef PARALLEL_INTEGRATION
		if (rd == CP0_REGISTER_SP_STATUS)
		{
			// Might be waiting for the CPU to set a signal bit on the STATUS register. Increment timeout
			RSP::MFC0_count[rt] += 1;
			if (RSP::MFC0_count[rt] >= RSP::SP_STATUS_TIMEOUT)
			{
				*RSP::rsp.SP_STATUS_REG |= SP_STATUS_HALT;
				return MODE_CHECK_FLAGS;
			}
		}
		else if (rd == CP0_REGISTER_SP_SEMAPHORE)
		{
			/* Test-and-set on read, as the hardware does. The core's
			 * read_rsp_regs() applies the set-on-read side effect only to
			 * CPU-bus reads of SP_SEMAPHORE; the RSP's own MFC0 shares the
			 * register word (cr[0x7] aliases SP_SEMAPHORE_REG) but bypasses
			 * the core, so without this the RSP and the CPU can both
			 * "acquire" the lock.
			 *
			 * Then halt so the RSP yields to the CPU at the acquire point;
			 * the run loop's "semaphore != 0" branch in parallel.cpp keeps
			 * the SP_STATUS timeout slow while the lock is held. */
			*rsp->cp0.cr[CP0_REGISTER_SP_SEMAPHORE] = 1;
			*RSP::rsp.SP_STATUS_REG |= SP_STATUS_HALT;
			return MODE_CHECK_FLAGS;
		}
#endif

		//if (rd == 4) // SP_STATUS_REG
		//   fprintf(stderr, "READING STATUS REG!\n");

		return MODE_CONTINUE;
	}

#define RSP_HANDLE_STATUS_WRITE(flag) \
	switch (rt & (SP_SET_##flag | SP_CLR_##flag)) \
	{ \
		case SP_SET_##flag: status |= SP_STATUS_##flag; break; \
		case SP_CLR_##flag: status &= ~SP_STATUS_##flag; break; \
		default: break; \
	}

	static inline int rsp_status_write(RSP::CPUState *rsp, uint32_t rt)
	{
		//fprintf(stderr, "Writing 0x%x to status reg!\n", rt);

		uint32_t status = *rsp->cp0.cr[CP0_REGISTER_SP_STATUS];

		RSP_HANDLE_STATUS_WRITE(HALT)
		RSP_HANDLE_STATUS_WRITE(SSTEP)
		RSP_HANDLE_STATUS_WRITE(INTR_BREAK)
		RSP_HANDLE_STATUS_WRITE(SIG0)
		RSP_HANDLE_STATUS_WRITE(SIG1)
		RSP_HANDLE_STATUS_WRITE(SIG2)
		RSP_HANDLE_STATUS_WRITE(SIG3)
		RSP_HANDLE_STATUS_WRITE(SIG4)
		RSP_HANDLE_STATUS_WRITE(SIG5)
		RSP_HANDLE_STATUS_WRITE(SIG6)
		RSP_HANDLE_STATUS_WRITE(SIG7)

		switch (rt & (SP_SET_INTR | SP_CLR_INTR))
		{
			case SP_SET_INTR: *rsp->cp0.irq |= 1; break;
			case SP_CLR_INTR: *rsp->cp0.irq &= ~1; break;
			default: break;
		}

		if (rt & SP_CLR_BROKE)
			status &= ~SP_STATUS_BROKE;

		*rsp->cp0.cr[CP0_REGISTER_SP_STATUS] = status;
		return ((*rsp->cp0.irq & 1) || (status & SP_STATUS_HALT)) ? MODE_CHECK_FLAGS : MODE_CONTINUE;
	}

#ifdef PARALLEL_INTEGRATION
	/* A transfer stays inside the 4 KiB bank bit 12 of the memory address
	 * picks, wrapping there; the skip falls between rows, not after the
	 * last one. Both length registers then read back the skip, a zero count
	 * and a 0xff8 length. */
	static void rsp_dma_finish(RSP::CPUState *rsp, uint32_t length_reg, uint32_t bank, uint32_t mem,
	                           uint32_t dram)
	{
		*rsp->cp0.cr[CP0_REGISTER_DMA_CACHE] = bank | (mem & 0xff8);
		*rsp->cp0.cr[CP0_REGISTER_DMA_DRAM] = dram & 0xfffff8;
		*rsp->cp0.cr[CP0_REGISTER_DMA_READ_LENGTH] = (length_reg & 0xff800000u) | 0xff8;
		*rsp->cp0.cr[CP0_REGISTER_DMA_WRITE_LENGTH] = (length_reg & 0xff800000u) | 0xff8;
	}

	static int rsp_dma_read(RSP::CPUState *rsp)
	{
		uint32_t length_reg = *rsp->cp0.cr[CP0_REGISTER_DMA_READ_LENGTH];
		uint32_t length = ((length_reg & 0xFFF) | 7) + 1;
		uint32_t skip = (length_reg >> 20) & 0xFF8;
		unsigned count = ((length_reg >> 12) & 0xFF) + 1;
		uint32_t bank = *rsp->cp0.cr[CP0_REGISTER_DMA_CACHE] & 0x1000;
		uint32_t mem = *rsp->cp0.cr[CP0_REGISTER_DMA_CACHE] & 0xff8;
		uint32_t dram = *rsp->cp0.cr[CP0_REGISTER_DMA_DRAM] & 0xfffff8;
		unsigned i, j;

#ifdef INTENSE_DEBUG
		fprintf(stderr, "DMA READ: (0x%x <- 0x%x) len %u, count %u, skip %u\n", bank | mem, dram,
		        length, count, skip);
#endif

		for (i = 0; i < count; i++)
		{
			if (i != 0)
				dram += skip;
			for (j = 0; j < length; j += 4)
			{
				uint32_t word = rsp->rdram[(dram & 0x7FFFFC) >> 2];
				uint32_t off = mem & 0xffc;

				if (bank)
				{
					/* invalidate IMEM */
					unsigned block = off / CODE_BLOCK_SIZE;
					rsp->dirty_blocks |= (0x3 << block) >> 1;
					rsp->imem[off >> 2] = word;
				}
				else
					rsp->dmem[off >> 2] = word;

				mem = (mem + 4) & 0xfff;
				dram += 4;
			}
		}

		rsp_dma_finish(rsp, length_reg, bank, mem, dram);

#ifdef INTENSE_DEBUG
		log_rsp_mem_parallel();
#endif
		return rsp->dirty_blocks ? MODE_CHECK_FLAGS : MODE_CONTINUE;
	}

	static void rsp_dma_write(RSP::CPUState *rsp)
	{
		uint32_t length_reg = *rsp->cp0.cr[CP0_REGISTER_DMA_WRITE_LENGTH];
		uint32_t length = ((length_reg & 0xFFF) | 7) + 1;
		uint32_t skip = (length_reg >> 20) & 0xFF8;
		unsigned count = ((length_reg >> 12) & 0xFF) + 1;
		uint32_t bank = *rsp->cp0.cr[CP0_REGISTER_DMA_CACHE] & 0x1000;
		uint32_t mem = *rsp->cp0.cr[CP0_REGISTER_DMA_CACHE] & 0xff8;
		uint32_t dram = *rsp->cp0.cr[CP0_REGISTER_DMA_DRAM] & 0xfffff8;
		unsigned i, j;

#ifdef INTENSE_DEBUG
		fprintf(stderr, "DMA WRITE: (0x%x <- 0x%x) len %u, count %u, skip %u\n", dram, bank | mem,
		        length, count, skip);
#endif

		for (i = 0; i < count; i++)
		{
			if (i != 0)
				dram += skip;
			for (j = 0; j < length; j += 4)
			{
				uint32_t off = mem & 0xffc;
				rsp->rdram[(dram & 0x7FFFFC) >> 2] = bank ? rsp->imem[off >> 2] : rsp->dmem[off >> 2];
				mem = (mem + 4) & 0xfff;
				dram += 4;
			}
		}

		rsp_dma_finish(rsp, length_reg, bank, mem, dram);
#ifdef INTENSE_DEBUG
		log_rsp_mem_parallel();
#endif
	}

#endif

	int RSP_MTC0(RSP::CPUState *rsp, unsigned rd, unsigned rt)
	{
		uint32_t val = rsp->sr[rt];

		switch (static_cast<CP0Registers>(rd & 15))
		{
		case CP0_REGISTER_DMA_CACHE:
			*rsp->cp0.cr[CP0_REGISTER_DMA_CACHE] = val & 0x1ff8;
			break;

		case CP0_REGISTER_DMA_DRAM:
			*rsp->cp0.cr[CP0_REGISTER_DMA_DRAM] = val & 0xfffff8;
			break;

		case CP0_REGISTER_DMA_READ_LENGTH:
			*rsp->cp0.cr[CP0_REGISTER_DMA_READ_LENGTH] = val;
#ifdef PARALLEL_INTEGRATION
			return rsp_dma_read(rsp);
#else
			return MODE_DMA_READ;
#endif

		case CP0_REGISTER_DMA_WRITE_LENGTH:
			*rsp->cp0.cr[CP0_REGISTER_DMA_WRITE_LENGTH] = val;
#ifdef PARALLEL_INTEGRATION
			rsp_dma_write(rsp);
#endif
			break;

		case CP0_REGISTER_SP_STATUS:
			return rsp_status_write(rsp, val);

		case CP0_REGISTER_SP_SEMAPHORE:
			// Any write to the semaphore register, regardless of value, sets it to 0 for the next read
			*rsp->cp0.cr[CP0_REGISTER_SP_SEMAPHORE] = 0;
			break;

		case CP0_REGISTER_CMD_START:
#ifdef INTENSE_DEBUG
			fprintf(stderr, "CMD_START 0x%x\n", val & 0xfffffff8u);
#endif
			*rsp->cp0.cr[CP0_REGISTER_CMD_START] = *rsp->cp0.cr[CP0_REGISTER_CMD_CURRENT] =
			    *rsp->cp0.cr[CP0_REGISTER_CMD_END] = val & 0xfffffff8u;
			break;

		case CP0_REGISTER_CMD_END:
#ifdef INTENSE_DEBUG
			fprintf(stderr, "CMD_END 0x%x\n", val & 0xfffffff8u);
#endif
			*rsp->cp0.cr[CP0_REGISTER_CMD_END] = val & 0xfffffff8u;

#ifdef PARALLEL_INTEGRATION
			RSP::rsp.ProcessRdpList();
#endif
			break;

		case CP0_REGISTER_CMD_CLOCK:
			*rsp->cp0.cr[CP0_REGISTER_CMD_CLOCK] = val;
			break;

		case CP0_REGISTER_CMD_STATUS:
			*rsp->cp0.cr[CP0_REGISTER_CMD_STATUS] &= ~(!!(val & 0x1) << 0);
			*rsp->cp0.cr[CP0_REGISTER_CMD_STATUS] |= (!!(val & 0x2) << 0);
			*rsp->cp0.cr[CP0_REGISTER_CMD_STATUS] &= ~(!!(val & 0x4) << 1);
			*rsp->cp0.cr[CP0_REGISTER_CMD_STATUS] |= (!!(val & 0x8) << 1);
			*rsp->cp0.cr[CP0_REGISTER_CMD_STATUS] &= ~(!!(val & 0x10) << 2);
			*rsp->cp0.cr[CP0_REGISTER_CMD_STATUS] |= (!!(val & 0x20) << 2);
			*rsp->cp0.cr[CP0_REGISTER_CMD_TMEM_BUSY] &= !(val & 0x40) * -1;
			*rsp->cp0.cr[CP0_REGISTER_CMD_CLOCK] &= !(val & 0x200) * -1;
			break;

		/* DMA_FULL and DMA_BUSY are read-only */
		case CP0_REGISTER_DMA_FULL:
		case CP0_REGISTER_DMA_BUSY:
		case CP0_REGISTER_CMD_CURRENT:
		case CP0_REGISTER_CMD_BUSY:
		case CP0_REGISTER_CMD_PIPE_BUSY:
		case CP0_REGISTER_CMD_TMEM_BUSY:
			break;

		default:
			*rsp->cp0.cr[rd & 15] = val;
			break;
		}

		return MODE_CONTINUE;
	}
}
