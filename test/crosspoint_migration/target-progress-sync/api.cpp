#include <type_traits>

#include "ProgressMapper.h"

static_assert(std::is_same_v<KOReaderPosition, SavedProgressPosition>);
void existingReaderCall(const std::shared_ptr<Epub>& epub, const CrossPointPosition& position) {
  KOReaderPosition wire = ProgressMapper::toKOReader(epub, position, PositionCoordinateSpace::SourceDocument);
  (void)wire.valid;
}
void syncCall(const std::shared_ptr<Epub>& epub, GfxRenderer& renderer, const KOReaderPosition& wire) {
  const auto mapped =
      ProgressMapper::toCrossPoint(epub, wire, renderer, 0, 10, PositionCoordinateSpace::SourceDocument);
  (void)mapped.valid;
}
