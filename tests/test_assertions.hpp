#pragma once

/*
 * Assertions guard for test binaries.
 *
 * CMake's Release build type appends -DNDEBUG to the compile command,
 * which turns every assert() in the test sources into a no-op. Tests
 * that rely on assert() then pass silently without actually checking
 * anything. This was the root cause of two CI-only failures during
 * development: the p2p_message_type_range test and the
 * genesis_canonical test both passed locally under
 * -DCMAKE_BUILD_TYPE=Release and failed in CI, which builds without a
 * build type (leaving NDEBUG undefined and asserts active).
 *
 * CMakeLists.txt now adds -UNDEBUG to every caesar_*_test target and
 * force-includes this header via -include. If the -UNDEBUG flag ever
 * disappears -- for example after a refactor of the test target loop
 * -- the test build fails immediately with the message below instead
 * of silently disabling checks again.
 *
 * Production targets (caesard) are not affected: they continue to
 * build with -DNDEBUG for speed.
 */
#ifdef NDEBUG
#error \
    "Test built with NDEBUG: assert() would be a no-op. CMakeLists.txt must pass -UNDEBUG to test targets."
#endif
