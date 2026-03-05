#include "rdp_data_structures.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>

using namespace RDP;

namespace
{
static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}
}

int main()
{
	check(sizeof(StaticRasterizationState) == 32u, "StaticRasterizationState size mismatch");
	check(sizeof(DepthBlendState) == 16u, "DepthBlendState size mismatch");
	check(sizeof(DerivedSetup) == 56u, "DerivedSetup size mismatch");
	check((sizeof(InstanceIndices) & 15u) == 0u, "InstanceIndices alignment mismatch");
	check((sizeof(UploadInfo) & 15u) == 0u, "UploadInfo alignment mismatch");
	check((sizeof(SpanSetup) & 15u) == 0u, "SpanSetup alignment mismatch");

	check(Limits::MaxNumTiles == 8u, "MaxNumTiles mismatch");
	check(ImplementationConstants::TileWidth == 8u, "TileWidth mismatch");
	check(ImplementationConstants::TileHeight == 8u, "TileHeight mismatch");

	StateCache<uint32_t, 4> state_cache;
	check(state_cache.empty(), "StateCache should begin empty");
	check(state_cache.add(10u) == 0u, "StateCache first index mismatch");
	check(state_cache.add(20u) == 1u, "StateCache second index mismatch");
	check(state_cache.size() == 2u, "StateCache size mismatch after inserts");
	check(state_cache.add(10u) == 0u, "StateCache dedup lookup mismatch");
	check(state_cache.size() == 2u, "StateCache size should not grow on dedup");
	check(state_cache.byte_size() == 2u * sizeof(uint32_t), "StateCache byte_size mismatch");
	check(!state_cache.full(), "StateCache should not be full");
	state_cache.add(30u);
	state_cache.add(40u);
	check(state_cache.full(), "StateCache full() mismatch");
	state_cache.reset();
	check(state_cache.empty(), "StateCache reset should clear data");

	StreamCache<uint16_t, 3> stream_cache;
	check(stream_cache.empty(), "StreamCache should begin empty");
	stream_cache.add(1u);
	stream_cache.add(2u);
	check(stream_cache.size() == 2u, "StreamCache size mismatch");
	check(stream_cache.byte_size() == 2u * sizeof(uint16_t), "StreamCache byte_size mismatch");
	stream_cache.add(3u);
	check(stream_cache.full(), "StreamCache full() mismatch");
	check(stream_cache.data()[0] == 1u && stream_cache.data()[2] == 3u, "StreamCache data mismatch");
	stream_cache.reset();
	check(stream_cache.empty(), "StreamCache reset should clear data");

	std::cout << "emu_unit_rdp_data_structures_test: PASS" << std::endl;
	return 0;
}
