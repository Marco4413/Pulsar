term.pushColor(term.yellow)
print("Workspace: pulsar")
term.popColor()
require("premake", ">=5.0.0-beta4")

newoption {
  trigger = "lsp-use-sanitizers",
  description = "Use sanitizers when building Debug",
  category = "Build Options"
}

local _arch = _OPTIONS["arch"]
if not _arch then
  _arch = os.hostarch()
  -- HACK: On my machine os.hostarch() returns x86 and not x86_64,
  --       probably related to Windows.
  if _arch == "x86" and os.is64bit() then
    _arch = "x86_64"
  end
end

workspace "pulsar"
  architecture (_arch)
  configurations { "Debug", "Release" }
  startproject "pulsar-tools"

include "projects/pulsar"
include "projects/pulsar-bindings"
include "projects/pulsar-debugger"
include "projects/pulsar-demo"

include "projects/pulsar-dap"
include "projects/pulsar-lsp"
include "projects/pulsar-tools"

include "projects/cpulsar"
