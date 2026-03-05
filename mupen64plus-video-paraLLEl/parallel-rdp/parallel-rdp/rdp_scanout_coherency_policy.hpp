#pragma once

namespace RDP
{
namespace detail
{
template <typename FlushAndSignalFn, typename ComputeRangeFn, typename ResolveCoherencyFn>
inline void run_scanout_coherency_sequence(bool is_host_coherent,
                                           FlushAndSignalFn &&flush_and_signal,
                                           ComputeRangeFn &&compute_scanout_memory_range,
                                           ResolveCoherencyFn &&resolve_coherency_external)
{
	flush_and_signal();
	if (is_host_coherent)
		return;

	unsigned offset = 0;
	unsigned length = 0;
	compute_scanout_memory_range(offset, length);
	resolve_coherency_external(offset, length);
}
}
}
