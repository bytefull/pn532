# Copyright (c) 2026 Bayrem Gharsellaoui
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_include_directories(../../include/zephyr/drivers/pn532)

zephyr_syscall_header(../../include/zephyr/drivers/pn532/pn532.h)

zephyr_library_sources(
  pn532.c
)
