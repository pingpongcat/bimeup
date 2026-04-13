#version 450

// Depth-only pass — no color output. The render pass has zero color attachments,
// so this shader just needs to be valid; fragments are kept for depth write only.
void main() {}
