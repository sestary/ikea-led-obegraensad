#pragma once

// Registers every previewable plugin, in src/main.cpp order.
//
// DDPPlugin and ArtNetPlugin are omitted: they render whatever an external
// controller pushes over UDP, so they have no effect of their own to preview.
//
// Safe to call more than once.
void simRegisterPlugins();
