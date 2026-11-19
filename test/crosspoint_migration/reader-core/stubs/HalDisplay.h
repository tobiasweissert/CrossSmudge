#pragma once
struct HostDisplay { int getDisplayWidth() const { return 64; } int getDisplayHeight() const { return 16; } };
inline HostDisplay display;
