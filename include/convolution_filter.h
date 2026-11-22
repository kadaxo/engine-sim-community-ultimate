#ifndef ATG_ENGINE_SIM_CONVOLUTION_FILTER_H
#define ATG_ENGINE_SIM_CONVOLUTION_FILTER_H

#include "filter.h"

#include <complex>
#include <vector>

// Convolution with an impulse response, one sample at a time and without
// delay. The first BlockSize taps are summed directly; the rest in blocks of
// BlockSize through the FFT (uniformly partitioned overlap-save), which a
// block's samples are ready for as soon as the block before has come in. The
// result is the direct convolution's, to float rounding, at about a
// twentieth of its cost for a 10000-tap response.
class ConvolutionFilter : public Filter {
    public:
        static constexpr int BlockSize = 128;

        ConvolutionFilter();
        virtual ~ConvolutionFilter();

        void initialize(int samples);
        virtual float f(float sample) override;
        virtual void destroy() override;

        int getSampleCount() const { return m_sampleCount; }

        // Filled in after initialize(), before the first f()
        float *getImpulseResponse() { return m_impulseResponse; }

    protected:
        void prepare();
        void processBlock();
        void fft(std::complex<float> *x, bool inverse) const;

        float *m_shiftRegister;     // the last BlockSize inputs, circular
        int m_shiftOffset;

        float *m_impulseResponse;
        int m_sampleCount;

        // The tail: the impulse response's partitions after the first, their
        // spectra, and the spectra of the input blocks they meet (newest
        // first, circular)
        bool m_prepared;
        int m_partitions;
        std::vector<std::complex<float>> m_partitionSpectra;   // [partition][bin]
        std::vector<std::complex<float>> m_inputSpectra;       // [block][bin]
        int m_newestInput;
        std::vector<float> m_previousBlock, m_currentBlock;
        std::vector<float> m_tailOutput;                        // this block's tail
        int m_blockPosition;
        std::vector<std::complex<float>> m_work;
        std::vector<std::complex<float>> m_twiddles;
        std::vector<int> m_bitReverse;
};

#endif /* ATG_ENGINE_SIM_CONVOLUTION_FILTER_H */
