Last Updated: 2026-11-10 06:41:13 UTC

# Changelog


<a name="v2026.11.9"></a>
## [v2026.11.9](https://github.com/carlkidcrypto/ezsnmp_data_logger/compare/v2026.11.7...v2026.11.9) (2026-11-09)


### Changes

#### Bug Fixes

- resolve SonarCloud security vulnerabilities and quality gate findings ([#45](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/45))


#### Chores

- bump version to 2026.11.9 and synchronize release touchpoints ([#46](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/46))


#### Documentation

- add versioned documentation for v2026.11.7 ([#37](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/37))


#### Features

- sync agentic workflows, custom agents, and skills with desktop release upload fix ([#38](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/38))

- standardize and compile against Net-SNMP 5.10 across native backends and workflows ([#44](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/44))

- configure Codecov test coverage uploads for Python and C++ native suites ([#39](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/39))

- add AI-assisted issue templates and mark winget as FUTURE/WIP ([#43](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/43))


#### Tests

- achieve 100% test coverage across supported languages and pin Codecov action ([#40](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/40))










---

<a name="v2026.11.7"></a>
## [v2026.11.7](https://github.com/carlkidcrypto/ezsnmp_data_logger/compare/v2026.11.6.1...v2026.11.7) (2026-11-07)


### Changes

#### Bug Fixes

- decouple desktop release asset upload and bump version to 2026.11.7 ([#36](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/36))


#### Documentation

- add versioned documentation for v2026.11.6.1 ([#35](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/35))










---

<a name="v2026.11.6.1"></a>
## [v2026.11.6.1](https://github.com/carlkidcrypto/ezsnmp_data_logger/compare/v2026.11.6...v2026.11.6.1) (2026-11-06)


### Changes

#### Bug Fixes

- standardize sphinx docs workflow on Python 3.14, pip caching, and targeted PR creation ([#32](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/32))


#### Chores

- bump version to 2026.11.6.1 for standalone desktop distributions and ecosystem parity ([#34](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/34))


#### Features

- add standalone Windows EXE, macOS DMG, and Linux DEB/RPM packaging ([#31](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/31))










---

<a name="v2026.11.6"></a>
## [v2026.11.6](https://github.com/carlkidcrypto/ezsnmp_data_logger/compare/v2026.11.5.2...v2026.11.6) (2026-11-06)


### Changes

#### Bug Fixes

- align npm workflow with purpleair-matterbridge OIDC parity and bump version to 2026.11.6 ([#30](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/30))










---

<a name="v2026.11.5.2"></a>
## [v2026.11.5.2](https://github.com/carlkidcrypto/ezsnmp_data_logger/compare/v2026.11.5.1...v2026.11.5.2) (2026-11-06)


### Changes

#### Bug Fixes

- ensure npm CLI >= 11 in active toolcache and remove conflicting registry-url for OIDC publishing ([#29](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/29))










---

<a name="v2026.11.5.1"></a>
## [v2026.11.5.1](https://github.com/carlkidcrypto/ezsnmp_data_logger/compare/v2026.11.5...v2026.11.5.1) (2026-11-05)


### Changes

#### Bug Fixes

- upgrade npm to latest, remove dotnet cache error, bump version to 2026.11.5.1, and update ezsnmp to v2026.11.4a0 ([#28](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/28))










---

<a name="v2026.11.5"></a>
## [v2026.11.5](https://github.com/carlkidcrypto/ezsnmp_data_logger/compare/v2026.11.3...v2026.11.5) (2026-11-05)


### Changes

#### Bug Fixes

- fix C# native compilation and publish gating across ecosystems ([#23](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/23))


#### Chores

- bump version to 2026.11.5 and optimize npm/nuget CI pipelines ([#27](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/27))


#### Documentation

- add versioned documentation for v2026.11.3 ([#22](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/22))


#### Features

- add NuGet OIDC Trusted Publishing authentication and environment binding ([#26](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/26))

- mimic purpleair-matterbridge with OIDC trusted publishing and provenance ([#24](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/24))


#### Performance Improvements

- enable npm dependency caching and parallel native addon compilation ([#25](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/25))










---

<a name="v2026.11.3"></a>
## v2026.11.3 (2026-11-04)


### Changes

#### Bug Fixes

- enable skip-existing for PyPI and TestPyPI publishing workflows ([#18](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/18))

- pre-provision third_party/ezsnmp in pypi workflows and add clone retries ([#17](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/17))

- fix cibuildwheel distribution repairs and remove invalid in-tree test command ([#15](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/15))

- allow graceful fallback when GitHub Pages is not enabled ([#2](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/2))

- add .nojekyll and sphinx.ext.githubpages for GitHub Pages deployment ([#9](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/9))


#### Build System

- bump the python-dependencies group across 1 directory with 2 updates ([#12](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/12))

- bump the github-actions group across 1 directory with 9 updates ([#13](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/13))


#### Chores

- bump version to 2026.11.3 for formal Python release ([#21](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/21))


#### Continuous Integration

- trigger test pypi deployment on main with smart change detection ([#20](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/20))

- implement smart workflow triggers and os-specific runner consolidation ([#16](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/16))

- configure sonarqube cloud automatic analysis and quality gate exclusions ([#14](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/14))

- configure dependabot for github actions, python, npm, nuget, and go ([#10](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/10))

- add NuGet and WinGet deployment pipelines and manifest generator ([#8](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/8))

- add PyPI, TestPyPI, and npm release distribution pipelines ([#7](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/7))


#### Features

- self-contained C++17 ezsnmp_data_logger (v2026.11.1a0) with SWIG bindings, CLI & cross-platform GUI ([#1](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/1))

- make plotted telemetry fields selectable and add multi-OS install & CLI/GUI guides ([#6](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/6))

- add custom branding icons, favicons, and OS application launchers ([#5](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/5))

- adopt key-centric locale.json format with English baseline ([#4](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/4))

- externalize UI strings to locale.json with multi-language support ([#3](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/3))


#### Performance Improvements

- enable multi-core compilation, build[uv], and C++ object caching ([#19](https://github.com/carlkidcrypto/ezsnmp_data_logger/issues/19))










---

