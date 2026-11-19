# Schema bridge dependencies

Unmodified upstream headers and LICENSE files only. No build-time network.

- Valijson v1.1.3: https://github.com/tristanpenman/valijson/tree/v1.1.3
  BSD-2-Clause. Archive SHA-256:
  `887b53b1a924f6fe0b35fa3bbc9bbbe5ae8c72097b7f0f7c17b45f4cfa646029`
  Download: https://codeload.github.com/tristanpenman/valijson/tar.gz/refs/tags/v1.1.3
- picojson v1.3.0: https://github.com/kazuho/picojson/tree/v1.3.0
  BSD-2-Clause. Archive SHA-256:
  `056805ca2691798f5545935a14bb477f2e1d827c9fb862e6e449dbea22801c7d`
  Download: https://codeload.github.com/kazuho/picojson/tar.gz/refs/tags/v1.3.0
  Build uses PICOJSON_USE_INT64 as required by Valijson's adapter.

Actual consumer: `pangu_services_schema` only, C++17 with public C ABI.
`pangu_services`, Core and other modules do not link C++ unless their actual
consumer selects this validator. Central dependency catalog is owned by the
integration role; these source-local vendored headers are staged here within
services ownership for immediate reproducible integration.

The C17 `schema-normalize.c` consumer now uses yyjson 0.12.0 (MIT, existing
Core JSON implementation) for owned argument cloning, JSON serialization and
numeric parsing. The strict schema engine remains this single Valijson bridge;
normalization does not implement a second JSON or JSON Schema validator.
`services_tools` selects this consumer for its default file/execution validators
and therefore now publicly links `services_schema`. Custom normalizing callbacks
can be supplied through the file/execution configuration. TypeBox 1.3.27 is only
an opt-in source-test dependency; no Node runtime is linked or shipped.
