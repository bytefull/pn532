# PN532

[![GitHub Build workflow status](https://github.com/bytefull/pn532/workflows/Build/badge.svg)](https://github.com/bytefull/pn532/actions/workflows/build.yml)
[![GitHub Test workflow status](https://github.com/bytefull/pn532/workflows/Test/badge.svg)](https://github.com/bytefull/pn532/actions/workflows/test.yml)
[![Coverage](https://codecov.io/gh/bytefull/pn532/graph/badge.svg)](https://codecov.io/gh/bytefull/pn532)
[![GitHub release](https://img.shields.io/github/v/release/bytefull/pn532)](https://github.com/bytefull/pn532/releases)
[![Zephyr RTOS](https://img.shields.io/badge/zephyr-v4.4.0-blue)](https://github.com/zephyrproject-rtos/zephyr/releases/tag/v4.4.0)
[![Zephyr SDK](https://img.shields.io/github/v/release/zephyrproject-rtos/sdk-ng?label=sdk-ng)](https://github.com/zephyrproject-rtos/sdk-ng/releases)
[![GitHub issues](https://img.shields.io/github/issues/bytefull/pn532)](https://github.com/bytefull/pn532/issues)
[![License](https://img.shields.io/github/license/bytefull/pn532)](https://github.com/bytefull/pn532/blob/main/LICENSE)

The PN532 Zephyr driver is an out-of-tree driver supporting the NXP PN532 NFC module. It provides a custom API for communicating with the module with extensibility for further NFC operations.

## ✅ TODO

- [x] Add PN532 GPIO control
- [ ] Add reset and irq gpio to driver and make them optional
- [ ] Refactor driver to prepare for unit tests
- [ ] Separate between unit tests and integration tests
- [ ] Make unit tests purely software
- [ ] Make integration tests use `native-pty-uart` to communicate with a python script that will send fake responses
