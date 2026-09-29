# rodin 4-8: mali r49p1 三件内建（LD2.0）挂载入口。
# 设备真值（vendor defconfig + blob 实证）：CONFIG_MTK_GPU_VERSION **空** ⇒
# midgard/Makefile:592-600 走 LD2.0 分支，内建对象名 = mali_kbase_mt6899_r49
# （与 vendor_dlkm 的 blob 名一致）。保持 GPU_VERSION 空还顺带关死 devmod
# drivers/gpu/mediatek/Makefile:56-64 对同源第二份 mali 的下探（防双编）。
export MTK_PLATFORM_VERSION := mt6899
# rodin 4-8: 激活 midgard/Makefile:167-259 的 mt6899 OOT 旋钮阶梯（≈50 个
# CONFIG_MALI_MTK_*，只在 BUILD_RULE=OOT 内生效），并让 Kbuild-mtk-custom-env:23
# 的 CONFIG_MALI_PLATFORM_NAME=MTK_PLATFORM 指到 mt6899 ⇒ midgard/Makefile:518
# 挂上 platform/mt6899/Kbuild（其中把 mali_kbase_config_mt6899.o 加进 mali_kbase-y）。
# 少了 export BUILD_RULE 会**静默**丢掉全部 MTK 特性（构建照样成功）。
export BUILD_RULE := OOT
export MTK_PLATFORM := mt6899
# rodin 4-8: 三件用真 Kconfig 门（rodin-ext/Kconfig.ext 内联 11 门）。midgard 自己的
# obj 行本来就是 obj-$(CONFIG_MALI_MIDGARD)（Makefile:595/598），=y 时自动进 built-in.a。
obj-$(CONFIG_MALI_MIDGARD) += mali_avalon/mali-r49p1/drivers/gpu/arm/midgard/
obj-$(CONFIG_MALI_MEMORY_GROUP_MANAGER) += mali_avalon/mali-r49p1/drivers/base/arm/memory_group_manager/
obj-$(CONFIG_MALI_PROTECTED_MEMORY_ALLOCATOR) += mali_avalon/mali-r49p1/drivers/base/arm/protected_memory_allocator/
# rodin 4-8: dma_buf_test_exporter 停编 —— 设备 vendor_dlkm 无 mali_dmabuf_test_* blob。
# rodin 4-8: 对齐设备构建面 —— blob 实证 kbase_page_migration_enabled 落在 .bss（初值 0
# = 显式关闭），这正是 __ANDROID_COMMON_KERNEL__ 被定义时的形态（未定义则为 -1 且落 .data）。
# 该宏同时决定 version_compat_defs.h 的 5 处分支（6.18 上两种取值等价，已逐条复核）。
subdir-ccflags-y += -D__ANDROID_COMMON_KERNEL__
