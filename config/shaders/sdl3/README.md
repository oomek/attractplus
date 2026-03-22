SDL3 GPU image pipeline shaders

The SDL3 backend looks for compiled image shader blobs at these default paths:

- `config/shaders/sdl3/image.vert.spv`
- `config/shaders/sdl3/alpha.frag.spv`
- `config/shaders/sdl3/add.frag.spv`
- `config/shaders/sdl3/subtract.frag.spv`
- `config/shaders/sdl3/screen.frag.spv`
- `config/shaders/sdl3/multiply.frag.spv`
- `config/shaders/sdl3/overlay.frag.spv`
- `config/shaders/sdl3/premultiplied.frag.spv`
- `config/shaders/sdl3/none.frag.spv`
- `config/shaders/sdl3/image.vert.dxil`
- `config/shaders/sdl3/alpha.frag.dxil`
- `config/shaders/sdl3/add.frag.dxil`
- `config/shaders/sdl3/subtract.frag.dxil`
- `config/shaders/sdl3/screen.frag.dxil`
- `config/shaders/sdl3/multiply.frag.dxil`
- `config/shaders/sdl3/overlay.frag.dxil`
- `config/shaders/sdl3/premultiplied.frag.dxil`
- `config/shaders/sdl3/none.frag.dxil`
- `config/shaders/sdl3/image.vert.metallib`
- `config/shaders/sdl3/alpha.frag.metallib`
- `config/shaders/sdl3/add.frag.metallib`
- `config/shaders/sdl3/subtract.frag.metallib`
- `config/shaders/sdl3/screen.frag.metallib`
- `config/shaders/sdl3/multiply.frag.metallib`
- `config/shaders/sdl3/overlay.frag.metallib`
- `config/shaders/sdl3/premultiplied.frag.metallib`
- `config/shaders/sdl3/none.frag.metallib`
- `config/shaders/sdl3/image.vert.msl`
- `config/shaders/sdl3/alpha.frag.msl`
- `config/shaders/sdl3/add.frag.msl`
- `config/shaders/sdl3/subtract.frag.msl`
- `config/shaders/sdl3/screen.frag.msl`
- `config/shaders/sdl3/multiply.frag.msl`
- `config/shaders/sdl3/overlay.frag.msl`
- `config/shaders/sdl3/premultiplied.frag.msl`
- `config/shaders/sdl3/none.frag.msl`

You can override the defaults with:

- `FE_SDL3_GPU_VERTEX_SHADER`
- `FE_SDL3_GPU_ALPHA_FRAGMENT_SHADER`
- `FE_SDL3_GPU_ADD_FRAGMENT_SHADER`
- `FE_SDL3_GPU_SUBTRACT_FRAGMENT_SHADER`
- `FE_SDL3_GPU_SCREEN_FRAGMENT_SHADER`
- `FE_SDL3_GPU_MULTIPLY_FRAGMENT_SHADER`
- `FE_SDL3_GPU_OVERLAY_FRAGMENT_SHADER`
- `FE_SDL3_GPU_PREMULTIPLIED_FRAGMENT_SHADER`
- `FE_SDL3_GPU_NONE_FRAGMENT_SHADER`

The source files in this folder define the expected vertex format and uniform layout:

- location `0`: `vec3` position
- location `1`: `vec2` texcoord
- location `2`: `vec4` color, normalized from `ubyte4`
- vertex uniform buffer slot `0`: projection matrix

To compile the default SPIR-V blobs on Windows:

```powershell
powershell -ExecutionPolicy Bypass -File util/compile-sdl3-shaders.ps1
```
