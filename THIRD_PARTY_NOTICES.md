# Third-Party Notices

Romantasy's native implementation is licensed as described in
`LICENSING.md`. Third-party components retain their own licenses.

- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) is included as
  a Git submodule and is licensed under GPL-3.0-or-later with its Modding
  Exception and GPL-3.0 Linking Exception. Romantasy's native DLL statically
  links CommonLibSSE-NG, so the combined native plugin is distributed under
  GPL-3.0-or-later with the applicable exceptions. The exact dependency
  revision and its complete license texts are available through the submodule.
- [nlohmann/json](https://github.com/nlohmann/json) is licensed under the MIT
  License.
- [toml++](https://github.com/marzer/tomlplusplus), version 3.4.0, is licensed
  under the MIT License. Its complete license text is in
  `licenses/tomlplusplus-MIT.txt`.
- [OStim Standalone](https://github.com/VersuchDrei/OStimNG)'s native plugin
  interface headers, tag 7.5.2, are copied unchanged into `src/ostim/api/`, with
  the notes in `src/ostim/THIRD_PARTY.md`. They are licensed under GPL-3.0. The
  licence text is in `src/ostim/api/LICENSE` and ships as
  `licenses/OStim-GPL-3.0.txt`. Romantasy finds OStim at run time and never loads
  or links its DLL.
- The public `Source/Scripts/Romantasy.psc` declaration file is separately
  available under the MIT License in `licenses/Romantasy-API-MIT.txt`.
- The ledger page bundles Latin subsets of three fonts, taken from the
  Fontsource packages (version 5.3.0) and distributed under the SIL Open Font
  License 1.1, whose complete text is in `licenses/OFL-1.1.txt`:
  - [Bodoni Moda](https://github.com/indestructible-type/Bodoni): Copyright 2020
    The Bodoni Moda Project Authors.
  - [Jost](https://github.com/indestructible-type/Jost): Copyright 2020 The
    Jost Project Authors.
  - [Pinyon Script](https://github.com/SorkinType/Pinyon): Copyright 2022 The
    PinyonScript Project Authors.
