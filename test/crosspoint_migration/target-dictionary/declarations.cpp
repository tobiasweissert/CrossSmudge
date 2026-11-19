#include <string>
#include <type_traits>
#include <vector>

#include "activities/reader/DictionaryDefinitionActivity.h"
#include "util/DictionaryRegistry.h"
#include "vocabulary/VocabularyStore.h"
static_assert(std::is_constructible_v<DictionaryDefinitionActivity, GfxRenderer&, MappedInputManager&,
                                      const std::string&, std::string>);
static_assert(std::is_constructible_v<DictionaryDefinitionActivity, GfxRenderer&, MappedInputManager&,
                                      const std::string&, std::string, bool>);
static_assert(std::is_constructible_v<DictionaryDefinitionActivity, GfxRenderer&, MappedInputManager&,
                                      const std::string&, const DictLocation&, bool, std::string>);
bool webEnumeration(const DictionaryRegistry& registry, std::vector<DictionaryEntry>& out) {
  return registry.discover(out);
}
bool staticWebEnumeration(std::vector<DictionaryEntry>& out) { return DictionaryRegistry::discover(out); }
