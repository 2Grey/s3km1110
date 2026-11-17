# Native protocol checks

Run from the repository root with a C++11 compiler. The small Arduino shim models
32-bit `millis()` and an in-memory `Stream`; no sensor or board is needed.

```sh
c++ -std=c++11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -g \
  -Itest/native -Iinclude src/s3km1110.cpp test/native/test.cpp \
  -o /tmp/s3km1110-tests
/tmp/s3km1110-tests
```

Repeat with `-DS3KM1110_SKIP_READ_CONFIG_ON_BEGIN` to check lightweight startup.
Use `-DS3KM1110_DEBUG_COMMANDS -DS3KM1110_DEBUG_DATA` to compile both debug paths.

Checks cover fragmented and consecutive reports, all 16 gate energies, footer
bytes inside payloads, malformed frames and recovery, exact outgoing commands,
configuration limits, text responses, failed ACKs, missing ACKs, initialization
failures, stream replacement, noise, and activity across `millis()` wraparound.
String responses with inconsistent declared lengths and rejected or missing
command-mode exit ACKs are also covered.

These checks validate the library's protocol handling; they do not replace a
board build or a test with the physical sensor.
