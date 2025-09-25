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
        "src",
        "vcpkg_installed/x64-windows/include"
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
      },
      "conditions": [
        ['OS=="win"', {
          "actions": [
            {
              "action_name": "vcpkg_install",
              "action_cwd": "<(module_root_dir)",
              "inputs": [ "vcpkg-configuration.json", "vcpkg.json" ],
              "outputs": [ "vcpkg_installed.stamp" ],
              "action": [
                "pwsh", "-NoLogo", "-NoProfile", "-File",
                "<(module_root_dir)/scripts/vcpkg.ps1"
              ]
            }
          ]
        }]
      ]
    }
  ]
}
