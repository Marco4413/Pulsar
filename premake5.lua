term.pushColor(term.yellow)
print("WARNING: IF YOU SEE THIS MESSAGE AND YOU ARE TRYING TO INCLUDE THE PULSAR PROJECT, YOU ARE ACTUALLY INCLUDING THE WORKSPACE FILE.")
print("      -> If so, make sure to include the pulsar folder instead.")
term.popColor()

newoption {
   trigger = "lsp-use-sanitizers",
   description = "Use sanitizers when building Debug",
   category = "Build Options"
}

newoption {
   trigger = "arch",
   value = "ARCH",
   description = "Choose architecture to build for",
   allowed = {
      { "amd64", "x86_64"
         },
      { "arm64", "ARM64"
         },
      { "default", "Chosen by the Build System"
         }
   },
   default = "default"
}

workspace "pulsar"
   -- architecture "x64"
   configurations { "Debug", "Release" }
   startproject "pulsar-tools"

   filter "options:arch=amd64"
      architecture "amd64"

   filter "options:arch=arm64"
      architecture "ARM64"

include "pulsar"
include "libs/fmt"
include "libs/lsp-framework/lspframework"

project "pulsar-demo"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   location "build/pulsar-demo"
   targetdir "%{prj.location}/%{cfg.buildcfg}"

   -- fmt is not needed by Pulsar, it's only used by pulsar-demo for logging
   includedirs { "include", "libs/fmt/include" }
   files "src/pulsar-demo/main.cpp"
   links { "pulsar", "fmt" }

   -- You can even tell Pulsar to not use the filesystem C++ header.
   -- Though it is used by pulsar-demo, so it would not work.
   -- defines "PULSAR_NO_FILESYSTEM"
   -- Moreover, you can disable the usage of atomics if you don't plan
   --  to use multi-threading (as of now, they're only used by SharedRef).
   -- defines "PULSAR_NO_ATOMIC"

   filter "toolset:clang"
      buildoptions {
         "-Wall", "-Wextra", "-Wpedantic", "-Werror",
         "-Wno-gnu-zero-variadic-macro-arguments"
      }

   filter "toolset:gcc"
      buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-Werror" }

   filter "action:vs*"
      flags { "FatalWarnings" }
      warnings "Extra"
      externalwarnings "Extra"

   filter "toolset:msc"
      buildoptions { "/W4", "/WX" }

   filter "configurations:Debug"
      defines "PULSAR_DEBUG"
      symbols "On"

   filter "configurations:Release"
      optimize "Speed"

project "pulsar-lsp"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   dependson "lspgen"

   location "build/pulsar-lsp"
   targetdir "%{prj.location}/%{cfg.buildcfg}"

   includedirs {
      "include",
      "libs/lsp-framework",
      "libs/lsp-framework/build/lspframework/generated"
   }
   files { "src/pulsar-lsp/**.cpp", "include/pulsar-lsp/**.h" }
   links { "pulsar", "lspframework" }

   filter "toolset:clang"
      buildoptions {
         "-Wall", "-Wextra", "-Wpedantic", "-Werror",
         "-Wno-gnu-zero-variadic-macro-arguments"
      }

   filter "toolset:gcc"
      buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-Werror" }

   filter "action:vs*"
      flags { "FatalWarnings" }
      warnings "Extra"
      externalwarnings "Extra"

   filter "toolset:msc"
      buildoptions { "/W4", "/WX" }

   filter { "configurations:Debug", "options:lsp-use-sanitizers" }
      buildoptions { "-fsanitize=address,undefined", "-fno-omit-frame-pointer" }
      linkoptions  { "-fsanitize=address,undefined" }

   filter "configurations:Debug"
      defines "PULSAR_DEBUG"
      symbols "On"

   filter "configurations:Release"
      optimize "Speed"

project "pulsar-tools"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   location "build/pulsar-tools"
   targetdir "%{prj.location}/%{cfg.buildcfg}"

   includedirs { "include", "libs/fmt/include", "libs/argue" }
   files {
      "src/pulsar-tools/**.cpp", "include/pulsar-tools/**.h",
      "libs/argue/argue.hpp"
   }
   links { "pulsar", "fmt" }

   filter "toolset:clang"
      buildoptions {
         "-Wall", "-Wextra", "-Wpedantic", "-Werror",
         "-Wno-gnu-zero-variadic-macro-arguments"
      }

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
