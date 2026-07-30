import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
PROFILE_ROOT = REPOSITORY_ROOT / "config" / "precu"
CLIENT_CONFIG = PROFILE_ROOT / "client.cfg"
LOGIN_CONFIG = PROFILE_ROOT / "precu_login.cfg"
LIVE_CONFIG = PROFILE_ROOT / "precu_live.cfg"
BLOOM_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/library/clientGame/src/shared/core/Bloom.cpp"
)
DIRECT3D_CONFIG_HEADER = REPOSITORY_ROOT / (
    "src/engine/client/application/Direct3d9/src/win32/ConfigDirect3d9.h"
)
DIRECT3D_CONFIG_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/application/Direct3d9/src/win32/ConfigDirect3d9.cpp"
)
DIRECT3D_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp"
)
DIRECT3D11_DYNAMIC_INDEX_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/application/Direct3d11/src/win32/"
    "Direct3d11_DynamicIndexBufferData.cpp"
)
STAGE_SCRIPT = REPOSITORY_ROOT / "scripts" / "Stage-X64Client.ps1"
BUILD_SCRIPT = REPOSITORY_ROOT / "scripts" / "Build-X64Client.ps1"

EXPECTED_TRE_ORDER = (
    "precu_runtime.tre",
    "precu_worlds.tre",
    "default_patch.tre",
    "patch_sku1_14_00.tre",
    "patch_14_00.tre",
    "patch_sku1_13_00.tre",
    "patch_13_00.tre",
    "patch_sku1_12_00.tre",
    "patch_12_00.tre",
    "patch_11_03.tre",
    "data_sku1_07.tre",
    "patch_11_02.tre",
    "data_sku1_06.tre",
    "patch_11_01.tre",
    "patch_11_00.tre",
    "data_sku1_05.tre",
    "data_sku1_04.tre",
    "data_sku1_03.tre",
    "data_sku1_02.tre",
    "data_sku1_01.tre",
    "data_sku1_00.tre",
    "patch_10.tre",
    "patch_09.tre",
    "patch_08.tre",
    "patch_07.tre",
    "patch_06.tre",
    "patch_05.tre",
    "patch_04.tre",
    "patch_03.tre",
    "patch_02.tre",
    "patch_01.tre",
    "patch_00.tre",
    "data_other_00.tre",
    "data_static_mesh_01.tre",
    "data_static_mesh_00.tre",
    "data_texture_07.tre",
    "data_texture_06.tre",
    "data_texture_05.tre",
    "data_texture_04.tre",
    "data_texture_03.tre",
    "data_texture_02.tre",
    "data_texture_01.tre",
    "data_texture_00.tre",
    "data_skeletal_mesh_01.tre",
    "data_skeletal_mesh_00.tre",
    "data_animation_00.tre",
    "data_sample_04.tre",
    "data_sample_03.tre",
    "data_sample_02.tre",
    "data_sample_01.tre",
    "data_sample_00.tre",
    "data_music_00.tre",
    "bottom.tre",
)


class PreCuRuntimeGraphicsContractTests(unittest.TestCase):
    def test_profile_owns_focus_free_local_acceptance_login(self):
        config = LOGIN_CONFIG.read_text(encoding="utf-8")
        self.assertRegex(config, r"(?m)^\s*loginClientID\s*=\s*1001\s*$")
        self.assertRegex(
            config,
            r"(?m)^\s*loginClientPassword\s*=\s*local\s*$",
        )

    def test_profile_owns_the_complete_publish14_include_chain(self):
        config = CLIENT_CONFIG.read_text(encoding="utf-8")
        includes = re.findall(r'^\.include\s+"([^"]+)"', config, re.MULTILINE)
        self.assertEqual(
            includes,
            [
                "precu_login.cfg",
                "precu_live.cfg",
                "precu_preload.cfg",
                "options.cfg",
                "user.cfg",
            ],
        )
        for include in includes[:-1]:
            self.assertTrue((PROFILE_ROOT / include).is_file(), include)

    def test_bloom_is_locked_off_after_user_preferences(self):
        config = CLIENT_CONFIG.read_text(encoding="utf-8")
        user_include = config.index('.include "user.cfg"')
        bloom_section = config.index("[ClientGame/Bloom]")
        self.assertGreater(bloom_section, user_include)
        self.assertRegex(
            config[bloom_section:],
            r"(?m)^\s*disable\s*=\s*true\s*$",
        )

    def test_live_manifest_is_the_canonical_53_tre_stack(self):
        config = LIVE_CONFIG.read_text(encoding="utf-8")
        trees = tuple(
            re.findall(r"(?m)^\s*searchTree_[^=]+\s*=\s*([^\s#]+)\s*$", config)
        )
        self.assertEqual(trees, EXPECTED_TRE_ORDER)
        self.assertIn("maxSearchPriority=27", config)
        self.assertRegex(
            config,
            r"(?m)^\s*searchTree_00_27=precu_runtime\.tre\s*$",
        )
        self.assertRegex(
            config,
            r"(?m)^\s*searchTree_00_26=precu_worlds\.tre\s*$",
        )
        self.assertNotIn("swgsource_3.0.tre", config.lower())

    def test_renderer_hard_override_cannot_be_reenabled_by_the_ui(self):
        source = BLOOM_SOURCE.read_text(encoding="utf-8")
        self.assertIn(
            'ConfigFile::getKeyBool ("ClientGame/Bloom", "disable", false)',
            source,
        )
        for function_name in ("setEnabled", "enable"):
            function = re.search(
                rf"void Bloom::{function_name}\([^)]*\)\s*\{{(.*?)\n\}}",
                source,
                re.DOTALL,
            )
            self.assertIsNotNone(function, function_name)
            self.assertIn("ms_disableViaConfig", function.group(1))

    def test_precu_staging_copies_and_records_the_profile(self):
        script = STAGE_SCRIPT.read_text(encoding="utf-8")
        self.assertIn('[ValidateSet("None", "Precu")]', script)
        self.assertIn('Join-Path $repoRoot "config\\precu"', script)
        self.assertIn(
            'Join-Path (Split-Path -Parent $repoRoot) "pre-cu-reborn-assets"',
            script,
        )
        for relative_path in (
            "ui\\ui_skill.inc",
            "datatables\\command\\command_table.iff",
            "datatables\\buff\\buff.iff",
            "datatables\\buff\\effect_mapping.iff",
            "datatables\\combat\\combat_data.iff",
        ):
            self.assertIn(f'"{relative_path}"', script)
        self.assertIn(
            'foreach ($archiveName in @("precu_runtime.tre", "precu_worlds.tre"))',
            script,
        )
        self.assertIn("Name='xpbar'", script)
        self.assertIn("@($precuAssetOverrideFiles)", script)
        self.assertIn("runtimeProfile   = $RuntimeProfile", script)
        self.assertIn("precuAssetOverrideCount", script)
        self.assertIn("precuAssetArchiveCount", script)

    def test_client_build_uses_bounded_parallelism(self):
        script = BUILD_SCRIPT.read_text(encoding="utf-8")
        self.assertIn("[int]$MaxCpuCount = 4", script)
        self.assertIn('"/m:$MaxCpuCount"', script)

    def test_dx11_index_ring_covers_expanded_water_tessellation(self):
        source = DIRECT3D11_DYNAMIC_INDEX_SOURCE.read_text(encoding="utf-8")
        capacity = re.search(r"cms_defaultIndices\s*=\s*(\d+)", source)
        self.assertIsNotNone(capacity)
        self.assertGreaterEqual(int(capacity.group(1)), 65536)

    def test_background_profile_cannot_activate_the_client_window(self):
        client = CLIENT_CONFIG.read_text(encoding="utf-8")
        options = (PROFILE_ROOT / "options.cfg").read_text(encoding="utf-8")
        config_header = DIRECT3D_CONFIG_HEADER.read_text(encoding="utf-8")
        config_source = DIRECT3D_CONFIG_SOURCE.read_text(encoding="utf-8")
        direct3d_source = DIRECT3D_SOURCE.read_text(encoding="utf-8")

        self.assertRegex(
            options,
            r"(?m)^\s*doNotActivateWindow\s*=\s*true\s*$",
        )
        user_include = client.index('.include "user.cfg"')
        profile_lock = client.index("doNotActivateWindow=true")
        self.assertGreater(profile_lock, user_include)
        self.assertIn("getDoNotActivateWindow", config_header)
        self.assertIn("KEY_BOOL(doNotActivateWindow, false)", config_source)
        self.assertIn(
            "ConfigDirect3d9::getDoNotActivateWindow() ? SWP_NOACTIVATE : 0",
            direct3d_source,
        )
        self.assertEqual(
            direct3d_source.count("SWP_SHOWWINDOW | windowActivationFlag"),
            2,
        )


if __name__ == "__main__":
    unittest.main()
