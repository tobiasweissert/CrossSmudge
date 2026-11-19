#pragma once
namespace ink::http {
class AllocationBudget;
}
namespace HalAirDropRadio {
// Checked Arduino owner release, without credential erasure or global teardown.
bool releaseArduino();
// Bind allocator accounting to only the receiver's owning TLS task.
bool beginTlsBudget(ink::http::AllocationBudget* budget);
void endTlsBudget();
}  // namespace HalAirDropRadio
