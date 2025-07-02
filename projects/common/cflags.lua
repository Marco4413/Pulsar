return function()
  filter "system:bsd"
    defines { "PULSAR_PLATFORM_UNIX", "PULSAR_PLATFORM_BSD" }

  filter "system:linux"
    defines { "PULSAR_PLATFORM_UNIX", "PULSAR_PLATFORM_LINUX" }

  filter "system:macosx"
    defines { "PULSAR_PLATFORM_UNIX", "PULSAR_PLATFORM_MACOSX" }

  filter "system:solaris"
    defines { "PULSAR_PLATFORM_UNIX", "PULSAR_PLATFORM_SOLARIS" }

  filter "system:windows"
    defines "PULSAR_PLATFORM_WINDOWS"

  filter "toolset:clang"
    buildoptions {
      "-Wall", "-Wextra", "-Wpedantic", "-Werror",
      "-Wno-gnu-zero-variadic-macro-arguments"
    }

  filter "toolset:gcc"
    buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-Werror" }

  filter "action:vs*"
    fatalwarnings { "All" }
    warnings "Extra"
    externalwarnings "Extra"

  filter "toolset:msc"
    buildoptions { "/W4", "/WX" }

  filter "configurations:Debug*"
    defines "PULSAR_DEBUG"
    symbols "On"

  filter "configurations:Release*"
    optimize "Speed"

  filter ""
end
