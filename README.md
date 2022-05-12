# HyperHDR for WebOS

Binaries are ready to install from [Homebrew Channel](https://repo.webosbrew.org/apps/org.webosbrew.hyperhdr.loader)

## Requirements

* Linux host system
* WebOS buildroot toolchain - arm-webos-linux-gnueabi_sdk-buildroot (https://github.com/openlgtv/buildroot-nc4/releases)
* git
* CMake
* npm

## Build

Build hyperhdr: `./build_hyperhdr.sh`

Build webOS frontend/service: `./build.sh`

Both scripts take an environment variable `TOOLCHAIN_DIR`, defaulting to: `$HOME/arm-webos-linux-gnueabi_sdk-buildroot`

To provide an individual path, call `export TOOLCHAIN_DIR=/your/toolchain/path` before executing respective scripts.

`build_hyperhdr.sh` also takes two other environment variables:

- `HYPERHDR_REPO`
- `HYPERHDR_BRANCH`

## References

[Hyperion.NG](https://github.com/hyperion-project/hyperion.ng)

[HyperHDR](https://github.com/awawa-dev/HyperHDR)

Video grabber of webOS: [hyperion-webos](https://github.com/webosbrew/hyperion-webos)

Frontend of Video grabber hyperion-webos: [piccap](https://github.com/TBSniller/piccap)

## Credits

@Smx-smx
@TBSniller
@Informatic
@Mariotaku
@Lord-Grey
@Paulchen-Panther
@Awawa
@chbartsch
