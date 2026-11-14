# Romantasy Licensing

Copyright (c) 2026 ColdSun.

Romantasy is distributed as `GPL-3.0-or-later` with the additional permissions in [EXCEPTIONS.md](EXCEPTIONS.md). This license covers the native implementation and the compiled `Romantasy.dll`.

Romantasy statically links CommonLibSSE-NG. A compiled native plugin containing CommonLibSSE-NG is a combined work and is not distributed as MIT-only software. The complete GNU GPL version 3 text is in [LICENSE](LICENSE). CommonLibSSE-NG retains its own license and exceptions in the `lib/commonlibsse-ng` submodule.

## Permissive public interfaces

The public Papyrus declaration file `Source/Scripts/Romantasy.psc` is separately available under the MIT License in `licenses/Romantasy-API-MIT.txt`. This allows follower authors to compile against the declarations without receiving an additional license requirement from that interface file. It does not relicense Romantasy's native implementation or any dependency linked by a consumer.

## OStim interface headers

The files under `src/ostim/api/` are OStim Standalone's own plugin interface headers, copied unchanged. They are under GPL-3.0 alone, with no additional permissions. The permissions in [EXCEPTIONS.md](EXCEPTIONS.md) are Romantasy's own and do not extend to those files. Their licence text is `src/ostim/api/LICENSE`, shipped as `licenses/OStim-GPL-3.0.txt`.

Fonts and other third-party material retain their own licenses. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The preferred form for modifying Romantasy is this repository with the CommonLibSSE-NG submodule initialized at the revision recorded by Git.

Binary release packages must include [LICENSE](LICENSE), [EXCEPTIONS.md](EXCEPTIONS.md), this file, [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), and the applicable API, font, and third-party license texts. Corresponding Source for the exact release, including its build files and pinned CommonLibSSE-NG revision, must accompany the binaries or be made available using a method permitted by GNU GPL version 3 section 6.
