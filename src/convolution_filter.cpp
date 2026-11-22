#include "../include/convolution_filter.h"

#include <algorithm>
#include <assert.h>
#include <cmath>
#include <string.h>

namespace {
    constexpr int FftSize = 2 * ConvolutionFilter::BlockSize;
}

ConvolutionFilter::ConvolutionFilter() {
    m_shiftRegister = nullptr;
    m_impulseResponse = nullptr;

    m_shiftOffset = 0;
    m_sampleCount = 0;

    m_prepared = false;
    m_partitions = 0;
    m_newestInput = 0;
    m_blockPosition = 0;
}

ConvolutionFilter::~ConvolutionFilter() {
    assert(m_shiftRegister == nullptr);
    assert(m_impulseResponse == nullptr);
}

void ConvolutionFilter::initialize(int samples) {
    m_sampleCount = samples;
    m_shiftOffset = 0;
    m_shiftRegister = new float[BlockSize];
    m_impulseResponse = new float[samples];

    memset(m_shiftRegister, 0, sizeof(float) * BlockSize);
    memset(m_impulseResponse, 0, sizeof(float) * samples);

    m_prepared = false;
}

void ConvolutionFilter::destroy() {
    delete[] m_shiftRegister;
    delete[] m_impulseResponse;

    m_shiftRegister = nullptr;
    m_impulseResponse = nullptr;

    m_prepared = false;
    m_partitionSpectra.clear();
    m_inputSpectra.clear();
}

// The impulse response is final by the first sample: its tail's partitions
// go to the frequency domain once
void ConvolutionFilter::prepare() {
    m_prepared = true;
    m_partitions = std::max(0, (m_sampleCount - 1) / BlockSize);
    m_blockPosition = 0;
    m_newestInput = 0;
    if (m_partitions == 0) return;

    m_twiddles.resize(FftSize / 2);
    for (int k = 0; k < FftSize / 2; ++k) {
        const double a = -2.0 * 3.14159265358979323846 * k / FftSize;
        m_twiddles[k] = std::complex<float>(static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a)));
    }
    m_bitReverse.resize(FftSize);
    int bits = 0;
    while ((1 << bits) < FftSize) ++bits;
    for (int i = 0; i < FftSize; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        m_bitReverse[i] = r;
    }

    const int bins = BlockSize + 1;
    m_work.assign(FftSize, 0.0f);
    m_partitionSpectra.assign(static_cast<size_t>(m_partitions) * bins, 0.0f);
    for (int p = 0; p < m_partitions; ++p) {
        std::fill(m_work.begin(), m_work.end(), std::complex<float>(0.0f));
        for (int u = 0; u < BlockSize; ++u) {
            const int t = (p + 1) * BlockSize + u;
            if (t < m_sampleCount) m_work[u] = m_impulseResponse[t];
        }
        fft(m_work.data(), false);
        std::copy(m_work.begin(), m_work.begin() + bins, m_partitionSpectra.begin() + static_cast<size_t>(p) * bins);
    }

    m_inputSpectra.assign(static_cast<size_t>(m_partitions) * bins, 0.0f);
    m_previousBlock.assign(BlockSize, 0.0f);
    m_currentBlock.assign(BlockSize, 0.0f);
    m_tailOutput.assign(BlockSize, 0.0f);
}

void ConvolutionFilter::fft(std::complex<float> *x, bool inverse) const {
    for (int i = 0; i < FftSize; ++i) {
        const int r = m_bitReverse[i];
        if (r > i) std::swap(x[i], x[r]);
    }

    for (int len = 2; len <= FftSize; len <<= 1) {
        const int half = len / 2;
        const int step = FftSize / len;
        for (int start = 0; start < FftSize; start += len) {
            for (int k = 0; k < half; ++k) {
                std::complex<float> w = m_twiddles[k * step];
                if (inverse) w = std::conj(w);
                const std::complex<float> a = x[start + k];
                const std::complex<float> b = w * x[start + k + half];
                x[start + k] = a + b;
                x[start + k + half] = a - b;
            }
        }
    }
}

// A block of input has come in: the tail's share of the next block's output.
// Partition j (taps jB to (j + 1)B) meets the input block j blocks back
// together with the one before it, an overlap-save segment of 2B, whose
// spectrum is kept from when it came in.
void ConvolutionFilter::processBlock() {
    const int bins = BlockSize + 1;
    for (int i = 0; i < BlockSize; ++i) {
        m_work[i] = m_previousBlock[i];
        m_work[BlockSize + i] = m_currentBlock[i];
    }
    fft(m_work.data(), false);

    m_newestInput = (m_newestInput + m_partitions - 1) % m_partitions;
    std::copy(m_work.begin(), m_work.begin() + bins,
        m_inputSpectra.begin() + static_cast<size_t>(m_newestInput) * bins);

    std::fill(m_work.begin(), m_work.end(), std::complex<float>(0.0f));
    for (int j = 0; j < m_partitions; ++j) {
        const std::complex<float> *in =
            &m_inputSpectra[static_cast<size_t>((m_newestInput + j) % m_partitions) * bins];
        const std::complex<float> *h = &m_partitionSpectra[static_cast<size_t>(j) * bins];
        for (int k = 0; k < bins; ++k) m_work[k] += in[k] * h[k];
    }

    // A real signal's spectrum is symmetric: the upper half from the lower
    for (int k = 1; k < BlockSize; ++k) m_work[FftSize - k] = std::conj(m_work[k]);
    fft(m_work.data(), true);

    const float scale = 1.0f / FftSize;
    for (int r = 0; r < BlockSize; ++r) m_tailOutput[r] = m_work[BlockSize + r].real() * scale;

    m_previousBlock.swap(m_currentBlock);
}

float ConvolutionFilter::f(float sample) {
    // Identity until initialize() allocates the shift register / impulse
    // response. Without this guard, rendering a Synthesizer before its
    // impulse responses are configured (e.g. before
    // initializeImpulseResponse() is called) dereferences the null
    // m_shiftRegister and crashes. A convolution with no impulse response
    // is the identity.
    if (m_shiftRegister == nullptr || m_sampleCount <= 0) {
        return sample;
    }
    if (!m_prepared) prepare();

    // The head: the first BlockSize taps, directly
    m_shiftRegister[m_shiftOffset] = sample;
    const int taps = std::min(m_sampleCount, BlockSize);
    const int wrap = BlockSize - m_shiftOffset;

    float result = 0;
    const int first = std::min(taps, wrap);
    for (int i = 0; i < first; ++i) {
        result += m_impulseResponse[i] * m_shiftRegister[i + m_shiftOffset];
    }
    for (int i = wrap; i < taps; ++i) {
        result += m_impulseResponse[i] * m_shiftRegister[i - wrap];
    }

    m_shiftOffset = (m_shiftOffset - 1 + BlockSize) % BlockSize;

    // The tail, worked out a block ahead
    if (m_partitions > 0) {
        result += m_tailOutput[m_blockPosition];
        m_currentBlock[m_blockPosition] = sample;
        if (++m_blockPosition == BlockSize) {
            processBlock();
            m_blockPosition = 0;
        }
    }

    return result;
}
