{
  "targets": [
    {
      "target_name": "pulsar",
      "type": "static_library",
      "include_dirs": [ "include" ],
      "sources": [
        "src/pulsar/bytecode.cpp",
        "src/pulsar/binary/reader.cpp",
        "src/pulsar/binary/writer.cpp",
        "src/pulsar/lexer.cpp",
        "src/pulsar/parser.cpp",
        "src/pulsar/runtime.cpp"
      ],
      "direct_dependent_settings": {
        "include_dirs": [ "include" ]
      },
      "conditions": [
        ["OS!=\"win\"", {
          "cflags": [
            "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Werror"
          ]
        }]
      ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "AdditionalOptions": [ "/std:c++20", "/W4", "/WX", "/analyze-" ],
        }
      }
    }
  ]
}