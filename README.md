# Zephyr driver for the PN532 NFC module

[![GitHub Build workflow status](https://github.com/bytefull/pn532/workflows/Build/badge.svg)](https://github.com/bytefull/pn532/actions/workflows/build.yml)
[![GitHub Test workflow status](https://github.com/bytefull/pn532/workflows/Test/badge.svg)](https://github.com/bytefull/pn532/actions/workflows/test.yml)
[![Coverage](https://codecov.io/gh/bytefull/pn532/branch/codecov/graph/badge.svg)](https://codecov.io/gh/bytefull/pn532)
[![GitHub release](https://img.shields.io/github/v/release/bytefull/pn532)](https://github.com/bytefull/pn532/releases)
[![Zephyr Compatibility](https://img.shields.io/badge/Zephyr-v4.1.0-blue)](https://github.com/zephyrproject-rtos/zephyr/releases/tag/v4.1.0)
[![GitHub issues](https://img.shields.io/github/issues/bytefull/pn532)](https://github.com/bytefull/pn532/issues)
![GitHub top language](https://img.shields.io/github/languages/top/bytefull/pn532)
[![License](https://img.shields.io/github/license/bytefull/pn532)](https://github.com/bytefull/pn532/blob/main/LICENSE)

The PN532 Zephyr driver is an out-of-tree module supporting the NXP PN532 NFC module. It provides a UART interface with a custom API, selectable via Kconfig, offering functionalities such as reading and writing NFC tags and initiating peer-to-peer communication with extensibility for further NFC operations.

## ✅ TODO

- [x] Build a minimal working driver with a single API

- [x] Add build CI + README badge

- [x] Add release CI + README badge

- [x] Add a unit test project to the module

- [x] Add test CI running on `native_sim` board + README badge

- [x] Add code coverage CI using `codecov` + README badge

- [x] Clean driver and prepare it for UART
