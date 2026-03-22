$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$shaderDir = Join-Path $root "config/shaders/sdl3"

$glslang = Get-Command glslangValidator.exe -ErrorAction SilentlyContinue
if (-not $glslang) {
	$glslang = Get-Command glslangValidator -ErrorAction SilentlyContinue
}

if (-not $glslang) {
	throw "glslangValidator was not found in PATH."
}

$jobs = @(
	@{ In = "image.vert.glsl"; Out = "image.vert.spv"; Stage = "vert" },
	@{ In = "image.frag.glsl"; Out = "image.frag.spv"; Stage = "frag" },
	@{ In = "alpha.frag.glsl"; Out = "alpha.frag.spv"; Stage = "frag" },
	@{ In = "add.frag.glsl"; Out = "add.frag.spv"; Stage = "frag" },
	@{ In = "subtract.frag.glsl"; Out = "subtract.frag.spv"; Stage = "frag" },
	@{ In = "screen.frag.glsl"; Out = "screen.frag.spv"; Stage = "frag" },
	@{ In = "multiply.frag.glsl"; Out = "multiply.frag.spv"; Stage = "frag" },
	@{ In = "overlay.frag.glsl"; Out = "overlay.frag.spv"; Stage = "frag" },
	@{ In = "premultiplied.frag.glsl"; Out = "premultiplied.frag.spv"; Stage = "frag" },
	@{ In = "none.frag.glsl"; Out = "none.frag.spv"; Stage = "frag" }
)

foreach ($job in $jobs) {
	$inputPath = Join-Path $shaderDir $job.In
	$outputPath = Join-Path $shaderDir $job.Out

	& $glslang.Source -V -S $job.Stage -o $outputPath $inputPath
	if ($LASTEXITCODE -ne 0) {
		throw "Failed compiling $($job.In)"
	}
}

Write-Host "SDL3 shaders compiled to $shaderDir"
