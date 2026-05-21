load("@mtk_connac2_wifi_build//:wifi_connac2_info.bzl", "srcs", "includes", "local_defines")
#repo:mtk_connac2_wifi_build init in bzlmod
#google link:https://bazel.build/rules/lib/globals/module?hl=zh-cn#use_repo_rule
#mtk solution in DTV branch:11055565

#load("//mtktv-modules:mtk_bazel/bzl/mtktv_ddk_ko.bzl", "define_mtktv_ddk_ko")
load("//build/bazel_mgk_rules:mgk_ddk_ko.bzl", "define_mgk_ddk_ko")

print(srcs)
print(includes)
print(local_defines)

config_opts = {
    "mt7961": {
        "MTK_COMBO_CHIP":"MT7961",
        "WM_RAM":"dtv",
        "CFG_PROJECT":"dtv_main",
        "CONFIG_MTK_COMBO_WIFI_HIF":"usb",
        "CONFIG_MTK_PREALLOC_MEMORY":"y",
        "CONFIG_CHIP_RESET_KO_SUPPORT":"y",
        "CONFIG_GKI_SUPPORT":"y",
        "MTK_ANDROID_VERSION":"16-base",
        "CONFIG_MTK_PLATFORM": "",
        "CONFIG_BUILD_RESETKO": "n",
        "CONFIG_BUILD_MTPREALLOC": "n",
        },
    "mt7926": {
        "MTK_COMBO_CHIP":"MT7926",
        "WM_RAM":"dtv",
        "CFG_PROJECT":"dtv_main",
        "CONFIG_MTK_COMBO_WIFI_HIF":"usb",
        "CONFIG_MTK_PREALLOC_MEMORY":"y",
        "CONFIG_CHIP_RESET_KO_SUPPORT":"y",
        "CONFIG_GKI_SUPPORT":"y",
        "MTK_ANDROID_VERSION":"16-base",
        "CONFIG_MTK_PLATFORM": "",
        "CONFIG_BUILD_RESETKO": "n",
        "CONFIG_BUILD_MTPREALLOC": "n",
        },
    "mt7902": {
        "MTK_COMBO_CHIP":"MT7902",
        "WM_RAM":"alps",
        "CFG_PROJECT":"",
        "CONFIG_MTK_COMBO_WIFI_HIF":"sdio",
        "CONFIG_MTK_PREALLOC_MEMORY":"y",
        "CONFIG_CHIP_RESET_KO_SUPPORT":"y",
        "CONFIG_GKI_SUPPORT":"y",
        "MTK_ANDROID_VERSION":"16-base",
        "CONFIG_MTK_PLATFORM": "",
        "CONFIG_BUILD_RESETKO": "n",
        "CONFIG_BUILD_MTPREALLOC": "n",
        },
}
warn_prefix = "-W"
no = "no-"

copts_def =[
    "-Werror",
    "-Wall",
    warn_prefix + no + "error=unused-but-set-variable",
    warn_prefix + no + "error=tautological-overlap-compare",
    warn_prefix + no + "cast-function-type-strict",
]

def gen4m_modules(platforms):
    define_mgk_ddk_ko(
        name = "mtreset_ko",
        module_type = "sub",
        srcs = [
            "reset/reset_fsm_def.c",
            "reset/reset_fsm.c",
            "reset/reset_hif.c",
            "reset/reset.c",
        ],
        includes = [
            "reset/include",
        ],
        hdrs = native.glob([
            "**/*.h"
        ]),
        local_defines = [
            "LINUX",
        ],
        copts = [],
        out = "mtreset.ko",
    )
    for p in platforms:
        print("list is: ", p)
        define_mgk_ddk_ko(
            name = "wlan_{}".format(p),
            module_type = "main",
            ko_deps = [
                ":wlan_{}_ko".format(p),
            ] + ([":mtreset_ko",] if config_opts[p]["CONFIG_CHIP_RESET_KO_SUPPORT"] == "y" else [])
              + ([":mtprealloc_{}_ko".format(p),] if config_opts[p]["CONFIG_MTK_PREALLOC_MEMORY"] == "y" else []),
        )
        define_mgk_ddk_ko(
            name = "mtprealloc_{}_ko".format(p),
            module_type = "sub",
            srcs = [
                "prealloc/prealloc.c",
            ],
            includes = includes[p],
            hdrs = native.glob([
                "**/*.h"
            ]),
            local_defines = local_defines[p],
            copts = copts_def,
            out = "mtprealloc_{}.ko".format(p)
        )

        define_mgk_ddk_ko(
            name = "wlan_{}_ko".format(p),
            module_type = "sub",
            srcs = srcs[p],
            includes = includes[p],
            hdrs = native.glob([
                "**/*.h"
            ]),
            local_defines = local_defines[p],
            copts = copts_def,
            out = "wlan_{}.ko".format(p)
        )
