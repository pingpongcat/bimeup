#version 450

// RP.17 feature-edge overlay — reuses the basic-mesh vertex buffer (pos+normal+color,
// stride 40) but only samples the position attribute. Normal/colour locations are
// ignored by the pipeline's vertex-input description.
layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
} camera;

// Matches the basic/opaque pipeline's push range so per-mesh model transforms
// place edge lines at the same position as their owning surface.
layout(push_constant) uniform PushConstants {
    layout(offset = 0) mat4 model;
} push;

void main() {
    gl_Position = camera.projection * camera.view * push.model * vec4(inPosition, 1.0);
}
