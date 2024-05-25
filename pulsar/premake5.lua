project "pulsar"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   location "../build/pulsar"
   targetdir "%{prj.location}/%{cfg.buildcfg}"

   includedirs "../include"
   files { "../src/pulsar/**.cpp", "../include/pulsar/**.h" }
   
   -- Define PULSAR_NO_FILESYSTEM to disable the usage of the FileSystem API.
   -- defines "PULSAR_NO_FILESYSTEM"
   -- You can also define PULSAR_NO_ATOMIC to disable the usage of the Atomic header.
   -- Atomics are mainly used by Pulsar::SharedRef to support multi-threading.
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
