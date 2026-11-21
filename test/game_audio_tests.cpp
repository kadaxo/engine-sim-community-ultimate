#include <gtest/gtest.h>

#include "../include/game_audio/audio_encoders.h"
#include "../include/game_audio/engine_session.h"
#include "../include/game_audio/exporter.h"
#include "../include/game_audio/json_writer.h"
#include "../include/game_audio/loop_builder.h"
#include "../include/game_audio/loudness.h"
#include "../include/game_audio/wav_file.h"

#ifdef ENGINE_SIM_EXPORT_VORBIS
#include <vorbis/vorbisfile.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>

namespace fs = std::filesystem;
using namespace game_audio;

namespace {

    constexpr double Pi = 3.14159265358979323846;

    // A harmonically rich periodic signal (like an engine firing pattern)
    // with a non-integer period, plus optional broadband noise.
    std::vector<float> periodicSignal(size_t n, double period, double noise, unsigned seed = 7) {
        std::mt19937 rng(seed);
        std::normal_distribution<double> gauss(0.0, 1.0);

        std::vector<float> out(n);
        for (size_t i = 0; i < n; ++i) {
            const double phase = 2 * Pi * i / period;
            double v = 0.5 * std::sin(phase)
                + 0.25 * std::sin(2 * phase + 0.3)
                + 0.15 * std::sin(4 * phase + 1.1)
                + 0.08 * std::sin(7 * phase + 2.0);
            v += noise * gauss(rng);
            out[i] = static_cast<float>(v);
        }
        return out;
    }

    double maxAbsStep(const std::vector<float> &x, size_t begin, size_t end) {
        double m = 0.0;
        for (size_t i = begin + 1; i < end; ++i) {
            m = std::max(m, std::abs(static_cast<double>(x[i]) - x[i - 1]));
        }
        return m;
    }

    fs::path tempDir() {
        const fs::path dir = fs::temp_directory_path() / "engine_sim_game_audio_tests";
        fs::create_directories(dir);
        return dir;
    }

    std::vector<uint8_t> readFile(const fs::path &p) {
        std::ifstream f(p, std::ios::binary);
        return std::vector<uint8_t>(
            (std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }

    uint32_t le32(const uint8_t *p) {
        return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }

    uint32_t be32(const uint8_t *p) {
        return (static_cast<uint32_t>(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    }

    const uint8_t *findBytes(const std::vector<uint8_t> &data, const char *needle, size_t limit) {
        const size_t n = std::strlen(needle);
        const size_t end = std::min(limit, data.size());
        for (size_t i = 0; i + n <= end; ++i) {
            if (std::memcmp(data.data() + i, needle, n) == 0) return data.data() + i;
        }
        return nullptr;
    }

} /* namespace */

// -------------------------------------------------------------------------
// Loop builder

TEST(LoopBuilderTests, CrossfadeGainsPreservePower) {
    for (const double rho : { 0.0, 0.3, 0.7, 1.0 }) {
        for (int i = 0; i <= 20; ++i) {
            const double t = i / 20.0;
            double a, b;
            crossfadeGains(t, rho, &a, &b);
            EXPECT_NEAR(a * a + b * b + 2 * rho * a * b, 1.0, 1e-9) << "rho=" << rho << " t=" << t;
        }

        double a, b;
        crossfadeGains(0.0, rho, &a, &b);
        EXPECT_NEAR(a, 0.0, 1e-12);
        EXPECT_NEAR(b, 1.0, 1e-12);
        crossfadeGains(1.0, rho, &a, &b);
        EXPECT_NEAR(a, 1.0, 1e-12);
        EXPECT_NEAR(b, 0.0, 1e-12);
    }
}

TEST(LoopBuilderTests, PeriodicLoopIsWholeCyclesAndSeamless) {
    const double period = 1234.567; // ~4286 rpm cycle at 44.1 kHz
    const std::vector<float> source = periodicSignal(44100 * 6, period, 0.01);

    LoopParameters p;
    p.periodSamples = period;
    p.targetLength = 44100 * 4;
    p.crossfadeLength = 3528;
    p.startOffset = 1000;

    LoopResult loop;
    std::string error;
    ASSERT_TRUE(buildSeamlessLoop(source, p, &loop, &error)) << error;

    const int expectedCycles = static_cast<int>(std::lround(p.targetLength / period));
    EXPECT_EQ(loop.cycles, expectedCycles);
    EXPECT_NEAR(static_cast<double>(loop.length), expectedCycles * period, 1.0);
    EXPECT_EQ(loop.samples.size(), loop.length);
    EXPECT_GT(loop.seamCorrelation, 0.99);

    // Play the loop twice in a row: the step across the seam must look like
    // any other step in the waveform.
    std::vector<float> twice = loop.samples;
    twice.insert(twice.end(), loop.samples.begin(), loop.samples.end());
    const double body = maxAbsStep(twice, 0, loop.length);
    const double seam = std::abs(static_cast<double>(twice[loop.length]) - twice[loop.length - 1]);
    EXPECT_LE(seam, body);
}

TEST(LoopBuilderTests, UnknownPeriodFindsAPeriodMultiple) {
    const double period = 777.3;
    const std::vector<float> source = periodicSignal(44100 * 4, period, 0.0);

    LoopParameters p;
    p.periodSamples = 0.0;
    p.targetLength = 44100 * 2;
    p.crossfadeLength = 2000;
    p.searchRadius = 2000;

    LoopResult loop;
    std::string error;
    ASSERT_TRUE(buildSeamlessLoop(source, p, &loop, &error)) << error;

    const double cycles = loop.length / period;
    EXPECT_NEAR(cycles, std::round(cycles), 0.01);
    EXPECT_GT(loop.seamCorrelation, 0.999);
}

TEST(LoopBuilderTests, StartSearchAvoidsADisturbedRegion) {
    const double period = 600.0;
    std::vector<float> source = periodicSignal(44100 * 4, period, 0.0);

    // Corrupt the first 0.2 s; with a start search the loop must begin
    // after it and still be perfectly periodic.
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> junk(-1.0f, 1.0f);
    for (size_t i = 0; i < 8820; ++i) source[i] = junk(rng);

    LoopParameters p;
    p.periodSamples = period;
    p.targetLength = 44100;
    p.crossfadeLength = 1200;
    p.startSearchLength = 22050;

    LoopResult loop;
    std::string error;
    ASSERT_TRUE(buildSeamlessLoop(source, p, &loop, &error)) << error;
    EXPECT_GE(loop.sourceStart, 8820u);
    EXPECT_GT(loop.seamCorrelation, 0.999);
}

TEST(LoopBuilderTests, RejectsShortSource) {
    LoopParameters p;
    p.periodSamples = 500.0;
    p.targetLength = 10000;
    p.crossfadeLength = 1000;

    const std::vector<float> source(requiredSourceLength(p) - 1, 0.5f);
    LoopResult loop;
    std::string error;
    EXPECT_FALSE(buildSeamlessLoop(source, p, &loop, &error));
    EXPECT_FALSE(error.empty());
}

TEST(LoopBuilderTests, DcRemoval) {
    std::vector<float> loop = periodicSignal(10000, 100.0, 0.0);
    for (float &s : loop) s += 0.3f;
    removeMean(loop);

    double mean = 0.0;
    for (const float s : loop) mean += s;
    EXPECT_NEAR(mean / loop.size(), 0.0, 1e-6);

    std::vector<float> oneShot(44100, 0.5f);
    dcBlock(oneShot, 44100.0, 5.0);
    EXPECT_LT(std::abs(oneShot.back()), 1e-3);
}

TEST(LoopBuilderTests, FadesAndLevels) {
    std::vector<float> x(1000, 1.0f);
    applyFadeIn(x, 100);
    applyFadeOut(x, 100);
    EXPECT_FLOAT_EQ(x.front(), 0.0f);
    EXPECT_NEAR(x.back(), 0.0f, 1e-6);
    EXPECT_FLOAT_EQ(x[500], 1.0f);

    EXPECT_NEAR(linearToDb(dbToLinear(-6.0)), -6.0, 1e-9);
    EXPECT_FLOAT_EQ(peakAbs({ 0.1f, -0.7f, 0.3f }), 0.7f);
}

// -------------------------------------------------------------------------
// Loudness (ITU-R BS.1770)

TEST(LoudnessTests, FullScaleSineReadsMinus3Lufs) {
    // BS.1770 reference: a 0 dBFS 997 Hz sine in one channel = -3.01 LUFS.
    for (const double fs : { 44100.0, 48000.0 }) {
        std::vector<float> sine(static_cast<size_t>(fs * 5));
        for (size_t i = 0; i < sine.size(); ++i) {
            sine[i] = static_cast<float>(std::sin(2 * Pi * 997.0 * i / fs));
        }
        EXPECT_NEAR(integratedLoudness(sine, fs), -3.01, 0.05) << "fs=" << fs;
    }
}

TEST(LoudnessTests, GatingIgnoresSilenceAndScalesWithGain) {
    std::vector<float> tone(44100 * 3);
    for (size_t i = 0; i < tone.size(); ++i) {
        tone[i] = static_cast<float>(0.5 * std::sin(2 * Pi * 997.0 * i / 44100.0));
    }
    const double reference = integratedLoudness(tone, 44100.0);

    // Half of this clip is silence. Ungated it would read ~3 LU lower; the
    // gates drop the silent blocks. The few blocks straddling the boundary
    // pass the relative gate (as BS.1770 specifies) and lower the result by
    // ~0.2 LU.
    std::vector<float> padded(44100 * 3, 0.0f);
    padded.insert(padded.end(), tone.begin(), tone.end());
    EXPECT_NEAR(integratedLoudness(padded, 44100.0), reference, 0.3);

    std::vector<float> quieter = tone;
    applyGain(quieter, dbToLinear(-12.0));
    EXPECT_NEAR(integratedLoudness(quieter, 44100.0), reference - 12.0, 0.01);

    EXPECT_FALSE(std::isfinite(integratedLoudness(std::vector<float>(44100, 0.0f), 44100.0)));
}

// -------------------------------------------------------------------------
// WAV

TEST(WavTests, Pcm16RoundTripWithLoopChunk) {
    const fs::path path = tempDir() / "roundtrip16.wav";

    std::vector<float> samples(5000);
    for (size_t i = 0; i < samples.size(); ++i) samples[i] = static_cast<float>(0.8 * std::sin(i * 0.01));

    WavWriteOptions options;
    options.bitsPerSample = 16;
    options.hasLoop = true;
    options.loopStart = 0;
    options.loopLength = static_cast<uint32_t>(samples.size());

    std::string error;
    ASSERT_TRUE(writeWavMono(path.string(), samples, 44100, options, &error)) << error;

    std::vector<int16_t> read;
    int rate = 0;
    ASSERT_TRUE(readWavPcm16Mono(path.string(), &read, &rate, &error)) << error;
    EXPECT_EQ(rate, 44100);
    ASSERT_EQ(read.size(), samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        // TPDF dither adds at most +-1 LSB, rounding another half.
        EXPECT_NEAR(read[i] / 32767.0, samples[i], 2.0 / 32767.0);
    }

    const std::vector<uint8_t> bytes = readFile(path);
    const uint8_t *smpl = findBytes(bytes, "smpl", bytes.size());
    ASSERT_NE(smpl, nullptr);
    EXPECT_EQ(le32(smpl + 8 + 28), 1u); // loop count
    EXPECT_EQ(le32(smpl + 8 + 36 + 8), 0u); // loop start
    EXPECT_EQ(le32(smpl + 8 + 36 + 12), samples.size() - 1); // inclusive end
}

TEST(WavTests, Pcm24HasExpectedSize) {
    const fs::path path = tempDir() / "size24.wav";
    const std::vector<float> samples(1001, 0.25f);

    WavWriteOptions options;
    options.bitsPerSample = 24;

    std::string error;
    ASSERT_TRUE(writeWavMono(path.string(), samples, 48000, options, &error)) << error;

    const std::vector<uint8_t> bytes = readFile(path);
    const uint8_t *data = findBytes(bytes, "data", bytes.size());
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(le32(data + 4), 1001u * 3);
    EXPECT_EQ(le32(bytes.data() + 4), bytes.size() - 8); // RIFF size incl. pad byte

    // Sample value 0.25 -> 0x200000 (little endian)
    EXPECT_EQ(data[8], 0x00);
    EXPECT_EQ(data[9], 0x00);
    EXPECT_EQ(data[10], 0x20);
}

TEST(WavTests, RejectsInvalidLoopAndBadFiles) {
    std::string error;
    WavWriteOptions options;
    options.hasLoop = true;
    options.loopStart = 10;
    options.loopLength = 100;
    EXPECT_FALSE(writeWavMono((tempDir() / "bad.wav").string(), std::vector<float>(50), 44100, options, &error));

    const fs::path junk = tempDir() / "junk.wav";
    std::ofstream(junk, std::ios::binary) << "not a wave file";
    std::vector<int16_t> samples;
    int rate = 0;
    EXPECT_FALSE(readWavPcm16Mono(junk.string(), &samples, &rate, &error));
    EXPECT_FALSE(readWavPcm16Mono((tempDir() / "missing.wav").string(), &samples, &rate, &error));
}

// -------------------------------------------------------------------------
// Encoders

#ifdef ENGINE_SIM_EXPORT_VORBIS
TEST(EncoderTests, OggVorbisIsSampleAccurateAndTagged) {
    const fs::path path = tempDir() / "loop.ogg";
    const std::vector<float> samples = periodicSignal(44100 * 2 + 123, 500.0, 0.01);

    OggVorbisOptions options;
    options.comments.push_back({ "LOOPSTART", "0" });
    options.comments.push_back({ "LOOPLENGTH", std::to_string(samples.size()) });

    std::string error;
    ASSERT_TRUE(encodeOggVorbis(path.string(), samples, 44100, options, &error)) << error;

    OggVorbis_File vf;
    ASSERT_EQ(ov_fopen(path.string().c_str(), &vf), 0);

    EXPECT_EQ(ov_pcm_total(&vf, -1), static_cast<ogg_int64_t>(samples.size()));
    EXPECT_EQ(ov_info(&vf, -1)->channels, 1);
    EXPECT_EQ(ov_info(&vf, -1)->rate, 44100);

    const char *loopLength = vorbis_comment_query(ov_comment(&vf, -1), "LOOPLENGTH", 0);
    ASSERT_NE(loopLength, nullptr);
    EXPECT_EQ(std::string(loopLength), std::to_string(samples.size()));

    // Decoded audio closely matches the input (lossy, but high quality).
    std::vector<float> decoded;
    for (;;) {
        float **pcm = nullptr;
        int section = 0;
        const long n = ov_read_float(&vf, &pcm, 4096, &section);
        if (n <= 0) break;
        decoded.insert(decoded.end(), pcm[0], pcm[0] + n);
    }
    ov_clear(&vf);

    ASSERT_EQ(decoded.size(), samples.size());
    EXPECT_GT(normalizedCrossCorrelation(decoded.data(), samples.data(), samples.size()), 0.99);
}

#endif /* ENGINE_SIM_EXPORT_VORBIS */

#ifdef ENGINE_SIM_EXPORT_MP3
TEST(EncoderTests, Mp3HasGaplessInfoTag) {
    const fs::path path = tempDir() / "loop.mp3";
    const std::vector<float> samples = periodicSignal(44100 * 2 + 777, 500.0, 0.01);

    std::string error;
    ASSERT_TRUE(encodeMp3(path.string(), samples, 44100, Mp3Options(), &error)) << error;

    const std::vector<uint8_t> bytes = readFile(path);
    ASSERT_GT(bytes.size(), 1000u);

    // First frame is the Xing/Info frame (VBR -> "Xing").
    ASSERT_EQ(bytes[0], 0xFF);
    ASSERT_EQ(bytes[1] & 0xE0, 0xE0);

    const uint8_t *xing = findBytes(bytes, "Xing", 2048);
    if (xing == nullptr) xing = findBytes(bytes, "Info", 2048);
    ASSERT_NE(xing, nullptr);

    const uint32_t flags = be32(xing + 4);
    ASSERT_TRUE(flags & 1u) << "frame count missing";
    const uint32_t frames = be32(xing + 8);

    const uint8_t *lame = findBytes(bytes, "LAME", 2048);
    ASSERT_NE(lame, nullptr);
    const uint32_t delay = (lame[21] << 4) | (lame[22] >> 4);
    const uint32_t padding = ((lame[22] & 0x0F) << 8) | lame[23];

    // Gapless decoders trim delay and padding to recover the exact length.
    EXPECT_EQ(static_cast<uint64_t>(frames) * 1152 - delay - padding, samples.size());
}
#endif /* ENGINE_SIM_EXPORT_MP3 */

// -------------------------------------------------------------------------
// JSON, plan and option handling

TEST(JsonWriterTests, ProducesNestedEscapedJson) {
    JsonWriter json;
    json.beginObject();
    json.key("name").value("a \"quoted\"\\path\n");
    json.key("list").beginArray().value(1).value(2.5).value(true).null().endArray();
    json.key("empty").beginObject().endObject();
    json.key("nan").value(std::nan(""));
    json.endObject();

    EXPECT_EQ(json.str(),
        "{\n"
        "  \"name\": \"a \\\"quoted\\\"\\\\path\\n\",\n"
        "  \"list\": [\n"
        "    1,\n"
        "    2.5,\n"
        "    true,\n"
        "    null\n"
        "  ],\n"
        "  \"empty\": {},\n"
        "  \"nan\": null\n"
        "}");

    EXPECT_EQ(JsonWriter::escape(std::string("\x01", 1)), "\\u0001");
}

TEST(ExporterTests, RpmGrid) {
    EXPECT_EQ(rpmGrid(1000, 3000, 500), (std::vector<double>{ 1000, 1500, 2000, 2500, 3000 }));
    EXPECT_EQ(rpmGrid(1000, 3200, 500), (std::vector<double>{ 1000, 1500, 2000, 2500, 3000, 3200 }));
    EXPECT_EQ(rpmGrid(1000, 3100, 500), (std::vector<double>{ 1000, 1500, 2000, 2500, 3000 }));
    EXPECT_TRUE(rpmGrid(3000, 1000, 500).empty());
}

TEST(ExporterTests, Slugify) {
    EXPECT_EQ(slugify("Subaru EJ25 (EH)"), "subaru_ej25_eh");
    EXPECT_EQ(slugify("  --  "), "engine");
    EXPECT_EQ(slugify("V8 - 5.7L"), "v8_5_7l");
}

TEST(ExporterTests, ValidateOptions) {
    ExportOptions o;
    o.session.scriptPath = "main.mr";
    EXPECT_EQ(validateOptions(o), "");

    ExportOptions bad = o;
    bad.writeOgg = bad.writeMp3 = bad.writeWav = false;
    EXPECT_NE(validateOptions(bad), "");

    bad = o;
    bad.clipGroups = { "turbo" };
    EXPECT_NE(validateOptions(bad), "");

    bad = o;
    bad.crossfadeSeconds = 2.0;
    EXPECT_NE(validateOptions(bad), "");

    bad = o;
    bad.rpmMin = 4000;
    bad.rpmMax = 3000;
    EXPECT_NE(validateOptions(bad), "");
}

// -------------------------------------------------------------------------
// Engine integration (uses the bundled default engine)

namespace {

    SessionConfig defaultEngineConfig() {
        const fs::path root = ENGINE_SIM_SOURCE_DIR;
        SessionConfig config;
        config.scriptPath = (root / "assets" / "main.mr").generic_string();
        config.searchPaths.push_back((root / "es").generic_string() + "/");
        return config;
    }

    std::vector<float> renderHeldRpm(double rpm, double throttle, double seconds) {
        std::string error;
        std::unique_ptr<EngineSession> session = EngineSession::create(defaultEngineConfig(), &error);
        EXPECT_TRUE(session) << error;
        if (!session) return {};

        session->setIgnition(true);
        session->setThrottleImmediate(throttle);
        for (int i = 0; i <= 50; ++i) {
            session->setDynoHold(rpm * i / 50.0);
            session->advance(0.01, nullptr, nullptr);
        }
        session->advance(1.0, nullptr, nullptr);

        std::vector<float> audio;
        session->advance(seconds, &audio, nullptr);
        EXPECT_NEAR(session->rpm(), rpm, rpm * 0.03);
        return audio;
    }

} /* namespace */

TEST(EngineSessionTests, HeldRpmProducesAudio) {
    std::srand(1);
    const std::vector<float> audio = renderHeldRpm(3000, 1.0, 0.5);

    // 44.1 kHz output for the simulated duration.
    EXPECT_NEAR(static_cast<double>(audio.size()), 0.5 * 44100, 44100 * 0.01);
    EXPECT_GT(rms(audio), 0.0);
    for (const float s : audio) ASSERT_TRUE(std::isfinite(s));
}

TEST(EngineSessionTests, RendersAreDeterministicForASeed) {
    std::srand(42);
    const std::vector<float> a = renderHeldRpm(2000, 0.5, 0.25);
    std::srand(42);
    const std::vector<float> b = renderHeldRpm(2000, 0.5, 0.25);
    EXPECT_EQ(a, b);
}

TEST(EngineSessionTests, OnLoadIsLouderThanOffLoad) {
    std::srand(3);
    const std::vector<float> on = renderHeldRpm(3000, 1.0, 0.5);
    std::srand(3);
    const std::vector<float> off = renderHeldRpm(3000, 0.0, 0.5);
    EXPECT_GT(rms(on), rms(off));
}

// Started from rest, the crank's first movement under the dyno hold is rounding
// noise. The hold used to follow its sign, so at some simulation frequencies
// (and with Apple Silicon's rounding) the engine ran backwards and made no power.
TEST(EngineSessionTests, DynoHoldRunsForwardAtAnySimulationFrequency) {
    for (const double frequency : { 10000.0, 20000.0, 40000.0, 80000.0 }) {
        std::srand(3);
        SessionConfig config = defaultEngineConfig();
        config.simulationFrequency = frequency;
        std::string error;
        std::unique_ptr<EngineSession> session = EngineSession::create(config, &error);
        ASSERT_TRUE(session) << error;

        session->setIgnition(true);
        session->setThrottleImmediate(1.0);
        for (int i = 0; i <= 50; ++i) {
            session->setDynoHold(3000 * i / 50.0);
            session->advance(0.01, nullptr, nullptr);
        }
        session->advance(0.5, nullptr, nullptr);

        std::vector<TelemetrySample> telemetry;
        session->advance(0.25, nullptr, &telemetry);
        ASSERT_FALSE(telemetry.empty());
        double torque = 0;
        for (const TelemetrySample &sample : telemetry) torque += sample.dynoTorque;
        torque /= telemetry.size();
        EXPECT_GT(torque, 100.0) << "at " << frequency << " Hz";
    }
}

TEST(EngineSessionTests, DynoHoldRunsCounterClockwiseEnginesForward) {
    // Ange's B18C runs counter-clockwise (a negative starter speed), as Honda
    // engines do. The hold must drive it that way, also when the starter speed
    // is set from outside the script (engine-sim-export --starter-rpm).
    const fs::path script = fs::path(ENGINE_SIM_SOURCE_DIR) / "assets" / "engines" / "atg-video-1" / "05_honda_vtec.mr";
    for (const double starterRpm : { 0.0, 300.0 }) {
        std::srand(3);
        SessionConfig config = defaultEngineConfig();
        config.scriptPath = script.generic_string();
        config.starterRpm = starterRpm;
        std::string error;
        std::unique_ptr<EngineSession> session = EngineSession::create(config, &error);
        ASSERT_TRUE(session) << error;

        session->setIgnition(true);
        session->setThrottleImmediate(1.0);
        for (int i = 0; i <= 50; ++i) {
            session->setDynoHold(3000 * i / 50.0);
            session->advance(0.01, nullptr, nullptr);
        }
        session->advance(0.5, nullptr, nullptr);

        std::vector<TelemetrySample> telemetry;
        session->advance(0.25, nullptr, &telemetry);
        ASSERT_FALSE(telemetry.empty());
        double torque = 0;
        for (const TelemetrySample &sample : telemetry) torque += sample.dynoTorque;
        torque /= telemetry.size();
        EXPECT_GT(torque, 50.0) << "starter " << starterRpm << " rpm";
        EXPECT_NEAR(session->rpm(), 3000, 3000 * 0.03) << "starter " << starterRpm << " rpm";
    }
}

TEST(EngineSessionTests, MissingScriptReportsError) {
    SessionConfig config = defaultEngineConfig();
    config.scriptPath = "this/does/not/exist.mr";
    std::string error;
    EXPECT_FALSE(EngineSession::create(config, &error));
    EXPECT_FALSE(error.empty());
}

TEST(EngineSessionTests, EngineFilesLoadDirectly) {
    const fs::path engines = fs::path(ENGINE_SIM_SOURCE_DIR) / "assets" / "engines";

    // One engine file that defines main() and an older one that only defines
    // its engine node
    for (const fs::path &file : {
        engines / "atg-video-2" / "07_gm_ls.mr",
        engines / "bmw" / "s54.mr" })
    {
        SessionConfig config = defaultEngineConfig();
        config.scriptPath = file.generic_string();

        std::string error;
        std::unique_ptr<EngineSession> session = EngineSession::create(config, &error);
        ASSERT_TRUE(session) << file << ": " << error;
        EXPECT_GT(session->info().cylinderCount, 0) << file;
    }
}

TEST(EngineSessionTests, ScriptWithoutEngineReportsError) {
    SessionConfig config = defaultEngineConfig();
    config.scriptPath = (fs::path(ENGINE_SIM_SOURCE_DIR) / "assets" / "themes" / "default.mr").generic_string();

    std::string error;
    EXPECT_FALSE(EngineSession::create(config, &error));
    EXPECT_NE(error.find("default.mr"), std::string::npos) << error;
}

// Forced induction, run in the example engines

#include "../include/engine.h"
#include "../include/forced_induction.h"
#include "../include/units.h"

namespace {

    SessionConfig forcedInductionConfig(const char *file) {
        SessionConfig config = defaultEngineConfig();
        config.scriptPath = (fs::path(ENGINE_SIM_SOURCE_DIR)
            / "assets" / "engines" / "forced-induction" / file).generic_string();
        return config;
    }

    std::unique_ptr<EngineSession> startForcedInduction(const char *file) {
        std::srand(1);
        std::string error;
        std::unique_ptr<EngineSession> session =
            EngineSession::create(forcedInductionConfig(file), &error);
        EXPECT_TRUE(session) << error;
        if (session) session->setIgnition(true);
        return session;
    }

    // Brings the dyno up to rpm at the given throttle
    void holdAt(EngineSession *session, double rpm, double throttle) {
        session->setThrottleImmediate(throttle);
        for (int i = 0; i <= 50; ++i) {
            session->setDynoHold(rpm * i / 50.0);
            session->advance(0.01, nullptr, nullptr);
        }
    }

    const ForcedInduction *forcedInduction(const EngineSession &session) {
        return session.engine()->getForcedInduction();
    }

    double psi(double pressure) {
        return pressure / units::psi;
    }

    // Power in one frequency (Goertzel), per sample
    double tonePower(const std::vector<float> &x, double f) {
        const double w = 2 * 3.14159265358979323846 * f / 44100.0;
        const double c = 2 * std::cos(w);
        double s1 = 0, s2 = 0;
        for (const float v : x) {
            const double s0 = v + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
        return p / (static_cast<double>(x.size()) * x.size());
    }

    // How far a tone stands out from the frequencies around it [dB]
    double toneProminence(const std::vector<float> &x, double f) {
        double around = 0;
        int n = 0;
        for (double d = 40; d <= 160; d += 10) {
            around += tonePower(x, f - d) + tonePower(x, f + d);
            n += 2;
        }
        return 10 * std::log10(tonePower(x, f) / (around / n));
    }

    // Mean power over f +- halfWidth, for a tone that wavers in pitch
    double bandPower(const std::vector<float> &x, double f, double halfWidth) {
        double sum = 0;
        int n = 0;
        for (double d = -halfWidth; d <= halfWidth; d += 5) {
            sum += tonePower(x, f + d);
            ++n;
        }
        return sum / n;
    }

    double meanSquare(const std::vector<float> &x) {
        double sum = 0;
        for (const float v : x) sum += static_cast<double>(v) * v;
        return sum / x.size();
    }

} /* namespace */

TEST(ForcedInductionEngineTests, TurboSpoolsDownAfterLiftAndLagReturns) {
    std::unique_ptr<EngineSession> session = startForcedInduction("subaru_ej25_twin_scroll_turbo.mr");
    ASSERT_TRUE(session);
    const ForcedInduction *fi = forcedInduction(*session);
    ASSERT_NE(fi, nullptr);

    holdAt(session.get(), 3000, 1.0);
    session->advance(2.0, nullptr, nullptr);
    const double loadedShaft = fi->getShaftSpeed();
    EXPECT_GT(psi(fi->getBoost()), 12.0);

    // Off the throttle the blow-off valve vents the charge and the wheel
    // slows down (it used to keep spinning at full speed for minutes)
    session->setThrottleImmediate(0.0);
    session->advance(1.0, nullptr, nullptr);
    EXPECT_LT(fi->getShaftSpeed(), 0.4 * loadedShaft);
    EXPECT_LT(psi(fi->getBoost()), 1.0);

    // So the next time the throttle opens, the boost has to build again
    session->setThrottleImmediate(1.0);
    session->advance(0.1, nullptr, nullptr);
    EXPECT_LT(psi(fi->getBoost()), 7.0);
}

TEST(ForcedInductionEngineTests, WastegateHoldsBoostWithoutHunting) {
    std::unique_ptr<EngineSession> session = startForcedInduction("subaru_ej25_twin_scroll_turbo.mr");
    ASSERT_TRUE(session);
    const ForcedInduction *fi = forcedInduction(*session);
    ASSERT_NE(fi, nullptr);
    const double target = psi(fi->getBoostTarget());

    holdAt(session.get(), 4500, 0.15);
    session->advance(1.0, nullptr, nullptr);

    // Snap the throttle open and watch the boost for 2.5 s
    session->setThrottleImmediate(1.0);
    double peak = 0, low = 1e9, high = 0, sum = 0;
    int n = 0;
    for (int i = 0; i < 125; ++i) {
        session->advance(0.02, nullptr, nullptr);
        const double boost = psi(fi->getBoost());
        peak = std::max(peak, boost);
        if (i >= 75) {
            low = std::min(low, boost);
            high = std::max(high, boost);
            sum += boost;
            ++n;
        }
    }

    EXPECT_LT(peak, target + 2.0) << "boost overshoots while the turbo spools";
    EXPECT_LT(high - low, 1.5) << "boost hunts between " << low << " and " << high << " psi";
    EXPECT_NEAR(sum / n, target, 1.0);
}

TEST(ForcedInductionEngineTests, RootsBlowerBoostsFromLowRpm) {
    std::unique_ptr<EngineSession> session = startForcedInduction("gm_ls_supercharged.mr");
    ASSERT_TRUE(session);
    const ForcedInduction *fi = forcedInduction(*session);
    ASSERT_NE(fi, nullptr);
    EXPECT_TRUE(fi->isPositiveDisplacement());

    holdAt(session.get(), 2000, 1.0);
    session->advance(1.5, nullptr, nullptr);
    const double low = psi(fi->getBoost());
    EXPECT_GT(low, 2.5);
    EXPECT_GT(fi->getDriveTorque(), 0.0);

    holdAt(session.get(), 4500, 1.0);
    session->advance(1.5, nullptr, nullptr);
    EXPECT_GT(psi(fi->getBoost()), low);
}

TEST(ForcedInductionEngineTests, SuperchargerWhinesAtTheRotorLobeFrequency) {
    std::unique_ptr<EngineSession> session = startForcedInduction("gm_ls_supercharged.mr");
    ASSERT_TRUE(session);
    const ForcedInduction *fi = forcedInduction(*session);
    ASSERT_NE(fi, nullptr);

    holdAt(session.get(), 4500, 1.0);
    session->advance(1.0, nullptr, nullptr);
    std::vector<float> on;
    session->advance(0.5, &on, nullptr);

    // A Roots rotor pack passes six lobes per turn
    const double lobes = 6 * fi->getShaftSpeed() / (2 * 3.14159265358979323846);
    EXPECT_GT(lobes, 800.0);
    EXPECT_GT(toneProminence(on, lobes), 15.0) << "no whine at " << lobes << " Hz";

    // Off the throttle the whine is a smaller part of the sound (it used to
    // drown out the engine)
    session->setThrottleImmediate(0.0);
    session->advance(1.0, nullptr, nullptr);
    std::vector<float> off;
    session->advance(0.5, &off, nullptr);

    const double onShare = 10 * std::log10(tonePower(on, lobes) / meanSquare(on));
    const double offShare = 10 * std::log10(tonePower(off, lobes) / meanSquare(off));
    EXPECT_LT(offShare, onShare - 3.0) << "on " << onShare << " dB, off " << offShare << " dB";
}

TEST(ForcedInductionEngineTests, TurboWhistleFadesAfterLift) {
    std::unique_ptr<EngineSession> session = startForcedInduction("subaru_ej25_twin_scroll_turbo.mr");
    ASSERT_TRUE(session);
    const ForcedInduction *fi = forcedInduction(*session);
    ASSERT_NE(fi, nullptr);

    // With its factory intake the compressor's once per revolution
    // whistle stays in; what gets out is the full blades passing
    auto pitch = [fi]() {
        return fi->getCompressorBlades() * fi->getShaftSpeed() / (2 * 3.14159265358979323846);
    };

    holdAt(session.get(), 4500, 1.0);
    session->advance(1.5, nullptr, nullptr);

    // The pitch follows the shaft speed, which moves with the boost, so
    // each 50 ms slice is measured at the pitch of its own moment
    double whistle = 0, around = 0, total = 0, meanPitch = 0;
    for (int i = 0; i < 10; ++i) {
        const double p0 = pitch();
        std::vector<float> slice;
        session->advance(0.05, &slice, nullptr);
        const double p = 0.5 * (p0 + pitch());
        meanPitch += p / 10;

        whistle += bandPower(slice, p, 20);
        double nearby = 0;
        int n = 0;
        // Clear of the band of air noise around the tone
        for (double d = 1500; d <= 2500; d += 50) {
            nearby += tonePower(slice, p - d) + tonePower(slice, p + d);
            n += 2;
        }
        around += nearby / n;
        total += meanSquare(slice);
    }
    EXPECT_GT(meanPitch, 8000.0);
    EXPECT_GT(10 * std::log10(whistle / around), 10.0) << "no whistle near " << meanPitch << " Hz";
    const double onTone = fi->getCompressorTones().bladePass;

    // After the throttle closes the turbo winds down and the whistle goes
    // (it used to stay at full pitch and volume)
    session->setThrottleImmediate(0.0);
    session->releaseDyno();
    session->advance(3.0, nullptr, nullptr);
    std::vector<float> after;
    session->advance(0.5, &after, nullptr);

    const double onShare = 10 * std::log10(whistle / total);
    const double afterShare = 10 * std::log10(bandPower(after, meanPitch, 150) / meanSquare(after));
    EXPECT_LT(afterShare, onShare - 15.0) << "on " << onShare << " dB, after " << afterShare << " dB";
    EXPECT_LT(fi->getCompressorTones().bladePass, 0.05 * onTone);
}

TEST(ForcedInductionEngineTests, DesignerEnginesReachTheirBoost) {
    // The engines from kyan0s-git's designer presets
    const struct {
        const char *file;
        double rpm;
        ForcedInduction::Arrangement arrangement;
    } engines[] = {
        { "toyota_2jz_gte.mr", 4500, ForcedInduction::Arrangement::Sequential },
        { "bmw_b58.mr", 3000, ForcedInduction::Arrangement::Single },
        // Where the wastegate runs nearly shut, which made the boost hunt
        // when the controller set the valve opening directly
        { "f1_2026_v6.mr", 9000, ForcedInduction::Arrangement::Single }
    };

    for (const auto &e : engines) {
        std::unique_ptr<EngineSession> session = startForcedInduction(e.file);
        ASSERT_TRUE(session) << e.file;
        const ForcedInduction *fi = forcedInduction(*session);
        ASSERT_NE(fi, nullptr) << e.file;
        EXPECT_TRUE(fi->isTurbocharger()) << e.file;
        EXPECT_EQ(fi->getArrangement(), e.arrangement) << e.file;

        holdAt(session.get(), e.rpm, 1.0);
        session->advance(1.5, nullptr, nullptr);

        double low = 1e9, high = 0;
        for (int i = 0; i < 50; ++i) {
            session->advance(0.02, nullptr, nullptr);
            low = std::min(low, psi(fi->getBoost()));
            high = std::max(high, psi(fi->getBoost()));
        }
        EXPECT_NEAR(0.5 * (low + high), psi(fi->getBoostTarget()), 1.0) << e.file;
        EXPECT_LT(high - low, 1.5) << e.file << ": boost hunts between " << low << " and " << high << " psi";
    }
}

// Diesel injection, run in the example turbodiesel

namespace {

    const char *const DieselExample = "diesel_2_0_common_rail_vgt.mr";

    // The example with some of its diesel_injection settings replaced, as a
    // script in the temporary folder
    SessionConfig dieselVariant(const char *name, const std::vector<std::pair<std::string, std::string>> &replace) {
        const fs::path source = fs::path(ENGINE_SIM_SOURCE_DIR) / "assets" / "engines" / "forced-induction" / DieselExample;
        std::ifstream in(source, std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        for (const auto &r : replace) {
            const size_t at = text.find(r.first);
            EXPECT_NE(at, std::string::npos) << r.first;
            if (at != std::string::npos) text.replace(at, r.first.size(), r.second);
        }

        const fs::path file = fs::temp_directory_path() / (std::string("engine-sim-diesel-") + name + ".mr");
        std::ofstream(file, std::ios::binary) << text;

        SessionConfig config = defaultEngineConfig();
        config.scriptPath = file.generic_string();
        return config;
    }

    std::unique_ptr<EngineSession> startDiesel(const SessionConfig &config) {
        std::srand(1);
        std::string error;
        std::unique_ptr<EngineSession> session = EngineSession::create(config, &error);
        EXPECT_TRUE(session) << error;
        if (!session) return session;

        // Started like a player does: the starter with the pedal up
        session->setIgnition(true);
        session->setThrottleImmediate(0.0);
        session->setStarter(true);
        EXPECT_TRUE(session->advanceUntil([](const EngineSession &s) { return s.rpm() > 600; }, 3.0, nullptr, nullptr));
        session->setStarter(false);
        return session;
    }

    // RMS between two frequencies, through two band-pass stages (one lets
    // through too much of the exhaust's much louder low end)
    double bandRms(const std::vector<float> &x, double lo, double hi) {
        const double f0 = std::sqrt(lo * hi), q = f0 / (hi - lo);
        const double w = 2 * 3.14159265358979323846 * f0 / 44100.0, a = std::sin(w) / (2 * q), a0 = 1 + a;
        const double b0 = a / a0, b2 = -a / a0, a1 = -2 * std::cos(w) / a0, a2 = (1 - a) / a0;
        std::vector<double> y(x.begin(), x.end());
        for (int stage = 0; stage < 2; ++stage) {
            double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
            for (double &v : y) {
                const double out = b0 * v + b2 * x2 - a1 * y1 - a2 * y2;
                x2 = x1; x1 = v; y2 = y1; y1 = out;
                v = out;
            }
        }

        double sum = 0;
        for (const double v : y) sum += v * v;
        return std::sqrt(sum / y.size());
    }

} /* namespace */

TEST(DieselEngineTests, IdlesOnItsGovernorWithoutVacuum) {
    std::unique_ptr<EngineSession> session = startDiesel(forcedInductionConfig(DieselExample));
    ASSERT_TRUE(session);
    ASSERT_NE(session->engine()->getDieselInjection(), nullptr);

    session->advance(6.0, nullptr, nullptr);
    EXPECT_NEAR(session->rpm(), 800, 60);

    // No throttle plates: the manifold stays at the outside pressure
    EXPECT_GT(session->engine()->getManifoldPressure(), units::pressure(95, units::kPa));
}

TEST(DieselEngineTests, OverrunCutsTheFuel) {
    std::unique_ptr<EngineSession> session = startDiesel(forcedInductionConfig(DieselExample));
    ASSERT_TRUE(session);
    const Engine *engine = session->engine();

    holdAt(session.get(), 2500, 1.0);
    session->advance(1.0, nullptr, nullptr);
    const double before = engine->getTotalFuelMassConsumed();
    session->advance(0.5, nullptr, nullptr);
    EXPECT_GT(engine->getTotalFuelMassConsumed(), before);

    // Pedal up above the idle speed: no fuel at all
    session->setThrottleImmediate(0.0);
    session->advance(0.5, nullptr, nullptr);
    const double coasting = engine->getTotalFuelMassConsumed();
    session->advance(0.5, nullptr, nullptr);
    EXPECT_EQ(engine->getTotalFuelMassConsumed(), coasting);

    // Let go of the dyno: it slows down to its idle, where the governor
    // starts injecting again
    session->releaseDyno();
    session->advance(4.0, nullptr, nullptr);
    EXPECT_NEAR(session->rpm(), 800, 80);
    EXPECT_GT(engine->getTotalFuelMassConsumed(), coasting);
}

TEST(DieselEngineTests, VariableGeometryTurboBoostsFromLowRpm) {
    std::unique_ptr<EngineSession> session = startDiesel(forcedInductionConfig(DieselExample));
    ASSERT_TRUE(session);
    const ForcedInduction *fi = forcedInduction(*session);
    ASSERT_NE(fi, nullptr);
    EXPECT_TRUE(fi->hasVariableGeometry());

    // At 1750 rpm the vanes close far enough to reach the full boost, then
    // open a little to hold it; there is no wastegate
    holdAt(session.get(), 1750, 1.0);
    session->advance(6.0, nullptr, nullptr);
    EXPECT_NEAR(psi(fi->getBoost()), psi(fi->getBoostTarget()), 1.5);
    EXPECT_GT(fi->getVaneOpening(), 0.0);
    EXPECT_LT(fi->getVaneOpening(), 1.0);
    EXPECT_EQ(fi->getWastegateOpening(), 0.0);

    // The same turbo with a fixed housing only gets there at higher rpm
    std::unique_ptr<EngineSession> fixed = startDiesel(dieselVariant("fixed", { { "variable_geometry: true", "variable_geometry: false" } }));
    ASSERT_TRUE(fixed);
    holdAt(fixed.get(), 1750, 1.0);
    fixed->advance(6.0, nullptr, nullptr);
    EXPECT_LT(psi(forcedInduction(*fixed)->getBoost()), 0.5 * psi(fi->getBoostTarget()));
}

TEST(DieselEngineTests, PilotInjectionSoftensTheKnock) {
    // The knock is the block ringing, in 1 to 3.5 kHz; it stands out most at
    // idle, where the exhaust is quiet
    auto knock = [](const SessionConfig &config) {
        std::unique_ptr<EngineSession> session = startDiesel(config);
        EXPECT_TRUE(session);
        if (!session) return 0.0;
        session->advance(3.0, nullptr, nullptr);
        std::vector<float> audio;
        session->advance(2.0, &audio, nullptr);
        return bandRms(audio, 1000, 3500) / std::sqrt(meanSquare(audio));
    };

    const double pilot = knock(forcedInductionConfig(DieselExample));
    const double noPilot = knock(dieselVariant("no-pilot", { { "pilot_injection: true", "pilot_injection: false" } }));
    const double silent = knock(dieselVariant("silent", {
        { "combustion_noise: 1.0", "combustion_noise: 0.0" },
        { "injector_noise: 1.0", "injector_noise: 0.0" } }));

    EXPECT_GT(20 * std::log10(pilot / silent), 3.0) << "the knock isn't heard";
    EXPECT_GT(20 * std::log10(noPilot / pilot), 3.0) << "the pilot injection doesn't soften it";
}
