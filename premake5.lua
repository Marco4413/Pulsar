term.pushColor(term.yellow)
print("WARNING: IF YOU SEE THIS MESSAGE AND YOU ARE TRYING TO INCLUDE THE PULSAR PROJECT, YOU ARE ACTUALLY INCLUDING THE WORKSPACE FILE.")
print("      -> If so, make sure to include the pulsar folder instead.")
term.popColor()

workspace "pulsar"
   -- architecture "x64"
   configurations { "Debug", "Release" }
   startproject "pulsar-tools"

include "pulsar"
include "libs/fmt"

project "pulsar-tools"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   location "build/pulsar-tools"
   targetdir "%{prj.location}/%{cfg.buildcfg}"

   includedirs { "include", "libs/fmt/include" }
   files { "src/pulsar-tools/**.cpp", "include/pulsar-tools/**.h" }
   links { "pulsar", "fmt" }
   
   -- Define PULSAR_NO_FILESYSTEM to disable the usage of the FileSystem API.
   -- defines "PULSAR_NO_FILESYSTEM"

   filter "toolset:gcc"
      buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-Werror" }

   filter "action:vs*"
      flags { "FatalWarnings" }
      warnings "Extra"
      externalwarnings "Extra"

   filter "toolset:msc"
      buildoptions { "/W4", "/WX" }

   filter "system:linux"
      links "pthread"

   filter "configurations:Debug"
      defines "PULSAR_DEBUG"
      symbols "On"

   filter "configurations:Release"
      optimize "Speed"

project "pulsar-dev"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   location "build/dev"
   targetdir "%{prj.location}/%{cfg.buildcfg}"

   includedirs { "include", "libs/fmt/include" }
   files { "src/dev/**.cpp", "src/dev/**.hpp", "include/dev/**.h" }
   links { "pulsar", "fmt" }

   filter "toolset:gcc"
      buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-Werror" }

   filter "toolset:msc"
      buildoptions { "/W4", "/WX" }

   filter "configurations:Debug"
      defines "PULSAR_DEBUG"
      symbols "On"

   filter "configurations:Release"
      optimize "Speed"
