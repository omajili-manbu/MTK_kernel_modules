# SPDX-License-Identifier: GPL-2.0
#
# rodin: BSP drop 不带本目录（DBMC 仲裁器未投），但 midgard/Makefile 的
# obj-$(CONFIG_MALI_ARBITRATION) += arbitration/ 仍在——make clean 对 obj- 桶
# 也下探（构建只下探 obj-y/m，故平时无感），缺 kbuild 文件即
# "No rule to make target .../arbitration/Makefile" 炸死整个 clean。
# 放一个空 Makefile 让下探可解析；本目录无任何源文件，永不产码。
