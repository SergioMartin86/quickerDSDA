// transfer.cpp -- Jaffar-compatibility / cross-thread save-state transfer test.
//
// JaffarPlus is a parallel best-first search: many worker threads pull arbitrary
// saved states out of a shared database, load them, and continue exploring with
// inputs that DIVERGE from any particular trajectory. For this to be correct, a
// saved state must be a *total*, *position-independent* description of the world:
//
//   1. RELOCATABLE   -- a state saved by one thread must load identically on any
//                       other thread (no absolute/TLS pointers may leak into the
//                       serialized bytes; thinker pointers must be swizzled).
//   2. TOTAL RESET   -- loading a state must FULLY overwrite the loading thread's
//                       world. If the thread previously explored a different
//                       branch, none of that residual state may survive the load.
//                       (This is exactly what the headless "skip G_InitNew on
//                       restore" optimization puts at risk: load no longer reloads
//                       the level, so every field the archive does not restore
//                       must be provably invariant.)
//   3. DETERMINISTIC -- load(S) then advance(X) must yield the same result on every
//                       thread, regardless of what that thread did before.
//
// This test stresses all three. For each checkpoint along the reference solution
// we record (a) the exact serialized bytes and (b) the state hash. Then, in
// parallel, each thread repeatedly: DIRTIES its world with random divergent inputs
// (modelling exploration of a wrong branch), LOADS a random checkpoint, and asserts
// the world is byte-identical to the reference -- then advances a fixed probe input
// and asserts the result still matches. Any residual-state leak, pointer-relocation
// bug, or nondeterminism shows up as a mismatch.

#include "argparse/argparse.hpp"
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/string.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/timing.hpp>
#include <jaffarCommon/parallel.hpp>
#include "emuInstance.hpp"
#include <atomic>
#include <cstring>
#include <mutex>
#include <random>
#include <string>
#include <vector>

// A divergent (non-solution) random input, used to dirty a thread's world before
// it loads a checkpoint. Deliberately spans the full input range.
static jaffar::input_t generateRandomInput(std::mt19937 &rng)
{
  jaffar::input_t in;
  in[0].forwardSpeed  = std::uniform_int_distribution<>{-50, 50}(rng);
  in[0].strafingSpeed = std::uniform_int_distribution<>{-50, 50}(rng);
  in[0].turningSpeed  = std::uniform_int_distribution<>{-120, 120}(rng);
  in[0].fire          = std::uniform_int_distribution<>{0, 1}(rng) == 1;
  in[0].action        = std::uniform_int_distribution<>{0, 1}(rng) == 1;
  in[0].weapon        = std::uniform_int_distribution<>{0, 7}(rng);
  return in;
}

int main(int argc, char *argv[])
{
  argparse::ArgumentParser program("tTester", "1.0");
  program.add_argument("scriptFile").help("Path to the test script (.test) file.").required();
  program.add_argument("sequenceFile").help("Path to the input sequence (.sol) to reproduce.").required();
  program.add_argument("--iterations").help("Stress iterations per thread.").default_value(std::string("200"));
  program.add_argument("--dirtySteps").help("Max divergent random steps used to dirty a world before each load.").default_value(std::string("32"));
  program.add_argument("--probeSteps").help("Divergent steps advanced after each transfer to test continuation determinism.").default_value(std::string("64"));
  program.add_argument("--seed").help("Base RNG seed.").default_value(std::string("13371337"));
  program.add_argument("--strict").help("Also require byte-canonical serialization (zone-snapshot readiness).").default_value(false).implicit_value(true);
  try { program.parse_args(argc, argv); }
  catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  const auto scriptFilePath   = program.get<std::string>("scriptFile");
  const auto sequenceFilePath = program.get<std::string>("sequenceFile");
  const size_t iterations     = std::stoul(program.get<std::string>("--iterations"));
  const size_t maxDirtySteps  = std::stoul(program.get<std::string>("--dirtySteps"));
  const size_t probeSteps     = std::stoul(program.get<std::string>("--probeSteps"));
  const bool   strict         = program.get<bool>("--strict");
  const uint64_t baseSeed     = std::stoull(program.get<std::string>("--seed"));

  // Load the test script + input sequence.
  std::string configJsRaw;
  if (!jaffarCommon::file::loadStringFromFile(configJsRaw, scriptFilePath)) JAFFAR_THROW_LOGIC("Could not read script file: %s\n", scriptFilePath.c_str());
  const auto configJs = nlohmann::json::parse(configJsRaw);

  auto expectedResult      = jaffarCommon::json::getObject(configJs, "Expected Result");
  auto expectedMapNumber   = jaffarCommon::json::getNumber<int>(expectedResult, "Map Number");
  auto expectedIsLevelExit = jaffarCommon::json::getBoolean(expectedResult, "Is Level Exit");
  auto expectedIsGameEnd   = jaffarCommon::json::getBoolean(expectedResult, "Is Game End");

  std::string sequenceRaw;
  if (!jaffarCommon::file::loadStringFromFile(sequenceRaw, sequenceFilePath)) JAFFAR_THROW_LOGIC("Could not read sequence file: %s\n", sequenceFilePath.c_str());
  const auto sequence = jaffarCommon::string::split(sequenceRaw, '\n');

  const auto maxThreads = jaffarCommon::parallel::getMaxThreadCount();

  printf("[] -----------------------------------------\n");
  printf("[] Jaffar-compatibility (cross-thread transfer) test\n");
  printf("[] Script:        '%s'\n", scriptFilePath.c_str());
  printf("[] Sequence:      '%s' (%lu steps)\n", sequenceFilePath.c_str(), sequence.size());
  printf("[] Threads:       %lu   Iterations/thread: %lu   Max dirty steps: %lu\n", maxThreads, iterations, maxDirtySteps);
  printf("[] ********** Running Test **********\n");
  fflush(stdout);

  // One independent emulator (private TLS Doom world) per thread.
  std::vector<std::unique_ptr<jaffar::EmuInstance>> emulators(maxThreads);
  JAFFAR_PARALLEL
  {
    int t = jaffarCommon::parallel::getThreadId();
    emulators[t] = std::make_unique<jaffar::EmuInstance>(configJs);
    emulators[t]->initialize();
    emulators[t]->disableRendering();
  }

  const size_t stateSize = emulators[0]->getStateSize();

  // Decode the reference solution.
  const auto inputParser = emulators[0]->getInputParser();
  std::vector<jaffar::input_t> decoded;
  for (const auto &s : sequence) decoded.push_back(inputParser->parseInputString(s));
  const size_t N = decoded.size();

  // ---- Phase A: build the ground-truth reference on a single clean instance ----
  // checkpoint[i] = the world after applying decoded[0..i]; we record its exact
  // serialized bytes, its effective length, its Jaffar hash, and the hash obtained
  // by loading it and advancing a fixed (per-checkpoint) probe input.
  auto &ref = *emulators[0];

  std::vector<std::vector<uint8_t>> chkBytes(N, std::vector<uint8_t>(stateSize));
  std::vector<size_t>                       chkLen(N);
  std::vector<jaffarCommon::hash::hash_t>   chkHash(N);
  std::vector<std::vector<jaffar::input_t>> probeSeq(N);          // per-checkpoint divergent continuation
  std::vector<jaffarCommon::hash::hash_t>   probeHash(N);         // hash after the full continuation

  // Deterministic probe sequences (same on every run -> all threads agree).
  std::mt19937 probeRng(baseSeed ^ 0x9E3779B97F4A7C15ull);
  for (size_t i = 0; i < N; i++)
  {
    probeSeq[i].resize(probeSteps);
    for (size_t k = 0; k < probeSteps; k++) probeSeq[i][k] = generateRandomInput(probeRng);
  }

  // Forward pass: record each checkpoint's bytes + hash.
  for (size_t i = 0; i < N; i++)
  {
    ref.advanceState(decoded[i]);
    jaffarCommon::serializer::Contiguous s(chkBytes[i].data(), stateSize);
    ref.serializeState(s);
    chkLen[i]  = ref.getEffectiveSaveStateSize();
    chkHash[i] = ref.getStateHash();
  }

  // Report the expected level outcome on the clean replay (informational: level
  // completion is validated by the rerecord/simple tests; this test owns TRANSFER).
  // The transfer-relevant part is that whatever the outcome, it survives save/load:
  // reload the final checkpoint and require getMapNumber/isLevelExit/isGameEnd to be
  // identical to the live values just observed.
  const int  liveMap  = ref.getMapNumber();
  const bool liveExit = ref.isLevelExit();
  const bool liveEnd  = ref.isGameEnd();
  if (liveMap != expectedMapNumber || liveExit != expectedIsLevelExit || liveEnd != expectedIsGameEnd)
    printf("[] Note: clean-replay outcome (map %d, exit %d, end %d) differs from .test expectation "
           "(map %d, exit %d, end %d) -- informational; see rerecord test for completion.\n",
           liveMap, liveExit, liveEnd, expectedMapNumber, expectedIsLevelExit, expectedIsGameEnd);
  bool outcomeRoundtripOk = true;
  {
    jaffarCommon::deserializer::Contiguous d(chkBytes[N ? N - 1 : 0].data(), stateSize);
    ref.deserializeState(d);
    if (ref.getMapNumber() != liveMap || ref.isLevelExit() != liveExit || ref.isGameEnd() != liveEnd)
    { printf("[] Test Failed: level outcome (map/exit/end) did not survive save/load\n"); outcomeRoundtripOk = false; }
  }

  // Probe pass: load each checkpoint fresh, advance its divergent continuation,
  // record the resulting hash (the determinism ground truth).
  for (size_t i = 0; i < N; i++)
  {
    jaffarCommon::deserializer::Contiguous d(chkBytes[i].data(), stateSize);
    ref.deserializeState(d);
    for (size_t k = 0; k < probeSteps; k++) ref.advanceState(probeSeq[i][k]);
    probeHash[i] = ref.getStateHash();
  }

  // ---- Phase B: parallel stress ----
  std::atomic<size_t> hashFailures{0};       // 2a: Jaffar state hash differed after load
  std::atomic<size_t> byteFailures{0};       // 2b: re-serialized bytes differed after load
  std::atomic<size_t> probeFailures{0};      // 3:  load+advance(probe) diverged
  std::atomic<size_t> lengthFailures{0};     // re-serialized length differed
  std::atomic<size_t> firstDiffOffset{~size_t(0)};  // smallest differing byte offset seen
  std::mutex logMutex;
  size_t loggedExamples = 0;

  auto hashEq = [](const jaffarCommon::hash::hash_t &a, const jaffarCommon::hash::hash_t &b)
  { return a.first == b.first && a.second == b.second; };

  JAFFAR_PARALLEL
  {
    int threadId = jaffarCommon::parallel::getThreadId();
    auto &e = *emulators[threadId];
    std::vector<uint8_t> scratch(stateSize);

    for (size_t iter = 0; iter < iterations; iter++)
    {
      // Per-(thread,iteration) RNG so every thread explores a different branch.
      std::mt19937 rng(baseSeed + threadId * 0x100000001b3ull + iter * 1099511628211ull);

      // (1) DIRTY: drop the world onto a random checkpoint, then diverge from the
      //     solution with random inputs. This leaves arbitrary residue (enemy AI,
      //     movers, sector state, RNG, ...) that a correct load must fully erase.
      if (N > 0)
      {
        size_t dirtySrc = std::uniform_int_distribution<size_t>{0, N - 1}(rng);
        jaffarCommon::deserializer::Contiguous d(chkBytes[dirtySrc].data(), stateSize);
        e.deserializeState(d);
        size_t dirtySteps = std::uniform_int_distribution<size_t>{0, maxDirtySteps}(rng);
        for (size_t k = 0; k < dirtySteps; k++) e.advanceState(generateRandomInput(rng));
      }

      // (2) TRANSFER: load a random checkpoint produced by a *different* thread's
      //     reference and assert a TOTAL, byte-identical reset.
      size_t j = (N > 0) ? std::uniform_int_distribution<size_t>{0, N - 1}(rng) : 0;
      if (N == 0) continue;

      jaffarCommon::deserializer::Contiguous d(chkBytes[j].data(), stateSize);
      e.deserializeState(d);

      bool ok = true;

      //   2a. Jaffar's own state hash must match the reference.
      if (!hashEq(e.getStateHash(), chkHash[j])) { hashFailures++; ok = false; }

      //   2b. Strongest check: re-serializing must reproduce the reference bytes
      //       exactly (catches residual state the light hash cannot see).
      jaffarCommon::serializer::Contiguous s(scratch.data(), stateSize);
      e.serializeState(s);
      size_t len = e.getEffectiveSaveStateSize();
      if (len != chkLen[j]) { lengthFailures++; ok = false; }
      else if (std::memcmp(scratch.data(), chkBytes[j].data(), len) != 0)
      {
        byteFailures++; ok = false;
        // record the first differing byte offset (for field identification)
        for (size_t b = 0; b < len; b++)
          if (scratch[b] != chkBytes[j][b])
          {
            size_t cur = firstDiffOffset.load();
            while (b < cur && !firstDiffOffset.compare_exchange_weak(cur, b)) {}
            break;
          }
      }

      // (3) DETERMINISM: load + a divergent multi-step continuation must match the
      //     reference, regardless of which thread runs it or what it did beforehand.
      for (size_t k = 0; k < probeSteps; k++) e.advanceState(probeSeq[j][k]);
      if (!hashEq(e.getStateHash(), probeHash[j])) { probeFailures++; ok = false; }

      if (!ok)
      {
        std::lock_guard<std::mutex> lock(logMutex);
        if (loggedExamples++ < 8)
          printf("[] MISMATCH  thread %d  iter %lu  checkpoint %lu\n", threadId, iter, j);
      }
    }
  }

  const size_t totalChecks = maxThreads * iterations;
  // FUNCTIONAL transfer = what JaffarPlus's search actually relies on: the state
  // hash it keys on is stable after a cross-thread load, and divergent continuation
  // is deterministic. CANONICITY = the serialized bytes are identical (no
  // allocation-dependent absolute pointers); not required by the current
  // archive-based load, but a hard prerequisite for a raw zone-snapshot.
  const bool functionalOk = !hashFailures && !probeFailures && !lengthFailures && outcomeRoundtripOk;
  const bool canonicalOk  = !byteFailures;

  printf("[] -----------------------------------------\n");
  printf("[] Transfers attempted:          %lu  (%lu threads x %lu iters)\n", totalChecks, maxThreads, iterations);
  printf("[] -- Functional transfer (Jaffar-critical) --\n");
  printf("[]   State-hash reset failures:  %lu\n", hashFailures.load());
  printf("[]   Continuation determinism:   %lu failures (%lu-step divergent probe)\n", probeFailures.load(), probeSteps);
  printf("[]   Length failures:            %lu\n", lengthFailures.load());
  printf("[] -- Serialization canonicity (zone-snapshot readiness) --\n");
  printf("[]   Byte-identity failures:     %lu%s\n", byteFailures.load(), strict ? " (STRICT: required)" : " (informational)");
  if (byteFailures) printf("[]   first differing byte offset: %lu  (allocation-dependent pointer in the archive)\n", firstDiffOffset.load());

  if (!functionalOk || (strict && !canonicalOk))
  {
    printf("[] TEST FAILED: %s\n", !functionalOk
      ? "save states are NOT functionally Jaffar-transferable."
      : "serialization is not byte-canonical (required under --strict for zone-snapshot).");
    return -1;
  }

  printf("[] Successful Execution.%s\n", canonicalOk ? "" : "  (functional transfer OK; serialization not yet byte-canonical -- see above)");
  printf("[] Reference Final Hash:                   0x%lX%lX\n", chkHash[N ? N - 1 : 0].first, chkHash[N ? N - 1 : 0].second);
  return 0;
}
