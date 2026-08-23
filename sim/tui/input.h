#pragma once

// Puts the terminal in raw, non-blocking mode on the alternate screen, and
// restores it afterwards. simInputEnd() is idempotent.
void simInputBegin();
void simInputEnd();

// Next pending key, or 0 if none is available.
char simInputPoll();
