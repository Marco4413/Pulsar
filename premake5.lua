term.pushColor(term.yellow)
print("Workspace: pulsar")
term.popColor()
require("premake", ">=5.0.0-beta4")

newoption {
  trigger = "lsp-use-sanitizers",
  description = "Use sanitizers when building Debug",
  category = "Build Options"
}

-- HACK: On my machine os.hostarch() returns x86 and not x86_64
local _desiredarch = _OPTIONS["arch"] or (os.hostarch() == "x86" and os.is64bit() and "x86_64")

workspace "pulsar"
if _desiredarch then
  architecture (_desiredarch)
end
  configurations { "Debug", "Release" }
  startproject "pulsar-tools"

include "projects/pulsar"
include "projects/pulsar-demo"
include "projects/pulsar-lsp"
include "projects/pulsar-tools"

include "projects/pulsar-core"
