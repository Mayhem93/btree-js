{
  "targets": [
    {
      "target_name": "btreejs",
      "sources": [
        "src/btree_addon.cpp",
        "src/btree.cpp",
        "src/btree_wrapper.cpp"
      ],
      "include_dirs": [
        "src"
      ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "Optimization": "MaxSpeed",
          "EnableIntrinsicFunctions": "true",
          "AdditionalOptions": [
            "/GL",
            "/fp:fast"
          ]
        },
        "Link": {
          "EnableCOMDATFolding": "true",
          "EnableFunctionLevelLinking": "true"
        }
      }

    }
  ]
}
