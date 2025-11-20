#ifndef RENDER_H
#define RENDER_H

// Initialize SDL, rendering system, and load assets
// Returns 0 on success, non-zero on failure
int Render_init(void);

// Main event loop - processes events and renders continuously
void Render_processEventsAndRender(void);

// Clean up and free all SDL resources
void Render_close(void);

// Render everything (called internally by processEventsAndRender)
void Render_renderEverything(void);

#endif // RENDER_H