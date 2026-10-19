#include "AnalysisEngine.h"

using namespace sx::analysis;

AnalysisEngine::AnalysisEngine() : juce::Thread ("SwitaxVoice analysis")
{
    fifoData.assign ((size_t) kFifoFrames * kFrameChannels, 0.0f);
    chunk.assign ((size_t) 4096 * kFrameChannels, 0.0f);
    const int n = fft.size();
    ringIn.assign ((size_t) n, 0.0f);
    ringOut.assign ((size_t) n, 0.0f);
    fftWork.assign ((size_t) n, {});
    window.resize ((size_t) n);
    for (int i = 0; i < n; ++i) window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * kPi * i / n);
    specIn.fill (-120.0f);
    specOut.fill (-120.0f);
    prepare (48000.0, 2);
    startThread (juce::Thread::Priority::low);
}

AnalysisEngine::~AnalysisEngine()
{
    cancelFlag = true;
    pool.removeAllJobs (true, 10000);
    stopThread (4000);
}

//==============================================================================
void AnalysisEngine::push (const float* inL, const float* inR, const float* outL, const float* outR,
                           const float* scL, const float* scR, int numSamples) noexcept
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite (numSamples, s1, n1, s2, n2);
    const int written = n1 + n2;
    if (written < numSamples) dropped.fetch_add (numSamples - written, std::memory_order_relaxed);

    auto write = [&] (int start, int count, int srcOffset)
    {
        for (int i = 0; i < count; ++i)
        {
            float* f = fifoData.data() + (size_t) (start + i) * kFrameChannels;
            const int k = srcOffset + i;
            f[0] = inL[k]; f[1] = inR[k]; f[2] = outL[k]; f[3] = outR[k];
            f[4] = scL != nullptr ? scL[k] : 0.0f;
            f[5] = scR != nullptr ? scR[k] : 0.0f;
        }
    };
    if (n1 > 0) write (s1, n1, 0);
    if (n2 > 0) write (s2, n2, n1);
    fifo.finishedWrite (written);
}

void AnalysisEngine::prepare (double newRate, int channels)
{
    const juce::ScopedLock sl (lock);
    sampleRate = newRate > 0 ? newRate : 48000.0;
    mainChannels = juce::jlimit (1, 2, channels);
    inLm.prepare (sampleRate, mainChannels);
    outLm.prepare (sampleRate, mainChannels);
    scLm.prepare (sampleRate, 2);
    outTp.reset();
    corrLL = corrRR = corrLR = 0; corrCount = 0; lastCorrelation = 1.0;
    // prepareToPlay never runs concurrently with processBlock, and the worker holds `lock`
    // while reading, so the FIFO can be reset safely here.
    fifo.reset();
    if (state.load() == State::Capturing) cancelLearning();
}

void AnalysisEngine::resetIntegrated()
{
    const juce::ScopedLock sl (lock);
    inLm.reset(); outLm.reset(); scLm.reset(); outTp.reset();
}

AnalysisEngine::LiveMeters AnalysisEngine::getLiveMeters() const
{
    const juce::ScopedLock sl (lock);
    LiveMeters m;
    m.inMomentary = inLm.momentary(); m.inShortTerm = inLm.shortTerm(); m.inIntegrated = inLm.integrated();
    m.outMomentary = outLm.momentary(); m.outShortTerm = outLm.shortTerm(); m.outIntegrated = outLm.integrated();
    m.outLra = outLm.loudnessRange();
    m.outTruePeak = outTp.truePeakDb();
    m.correlation = lastCorrelation;
    m.scShortTerm = scLm.shortTerm();
    m.sidechainActive = scSilentFrames < (int) sampleRate;   // signal within the last second
    m.droppedFrames = dropped.load();
    return m;
}

void AnalysisEngine::getDisplaySpectra (std::array<float, kDisplayPoints>& in, std::array<float, kDisplayPoints>& out) const
{
    const juce::ScopedLock sl (lock);
    in = specIn;
    out = specOut;
}

//==============================================================================
void AnalysisEngine::run()
{
    while (! threadShouldExit())
    {
        {
            const juce::ScopedLock sl (lock);
            for (int guard = 0; guard < 64; ++guard)
            {
                int s1, n1, s2, n2;
                const int want = juce::jmin (4096, fifo.getNumReady());
                if (want <= 0) break;
                fifo.prepareToRead (want, s1, n1, s2, n2);
                std::copy (fifoData.begin() + (ptrdiff_t) s1 * kFrameChannels, fifoData.begin() + (ptrdiff_t) (s1 + n1) * kFrameChannels, chunk.begin());
                if (n2 > 0)
                    std::copy (fifoData.begin() + (ptrdiff_t) s2 * kFrameChannels, fifoData.begin() + (ptrdiff_t) (s2 + n2) * kFrameChannels,
                               chunk.begin() + (ptrdiff_t) n1 * kFrameChannels);
                fifo.finishedRead (n1 + n2);
                processChunk (chunk.data(), n1 + n2);
            }
        }
        wait (15);
    }
}

void AnalysisEngine::processChunk (const float* frames, int numFrames)
{
    // de-interleave into small stack buffers (bounded chunk size)
    static constexpr int kMax = 4096;
    float il[kMax], ir[kMax], ol[kMax], orr[kMax], sl[kMax], sr2[kMax];
    numFrames = juce::jmin (numFrames, kMax);
    float scPeak = 0.0f;
    for (int i = 0; i < numFrames; ++i)
    {
        const float* f = frames + (size_t) i * kFrameChannels;
        il[i] = f[0]; ir[i] = f[1]; ol[i] = f[2]; orr[i] = f[3]; sl[i] = f[4]; sr2[i] = f[5];
        scPeak = juce::jmax (scPeak, std::abs (f[4]), std::abs (f[5]));
        corrLL += (double) f[2] * f[2]; corrRR += (double) f[3] * f[3]; corrLR += (double) f[2] * f[3];
    }
    scSilentFrames = scPeak > 1.0e-4f ? 0 : juce::jmin (1 << 30, scSilentFrames + numFrames);

    const float* inPtr[2] = { il, ir };
    const float* outPtr[2] = { ol, orr };
    const float* scPtr[2] = { sl, sr2 };
    inLm.process (inPtr, mainChannels, numFrames);
    outLm.process (outPtr, mainChannels, numFrames);
    scLm.process (scPtr, 2, numFrames);
    outTp.process (outPtr, mainChannels, numFrames);

    corrCount += numFrames;
    if (corrCount >= (int) (0.3 * sampleRate))
    {
        lastCorrelation = (corrLL > 1e-9 && corrRR > 1e-9) ? corrLR / std::sqrt (corrLL * corrRR) : 1.0;
        if (mainChannels == 1) lastCorrelation = 1.0;
        corrLL = corrRR = corrLR = 0; corrCount = 0;
    }

    // display spectra: 4096-point FFT every 2048 samples, smoothed in time
    const int n = fft.size();
    for (int i = 0; i < numFrames; ++i)
    {
        ringIn[(size_t) ringPos] = 0.5f * (il[i] + ir[i]);
        ringOut[(size_t) ringPos] = 0.5f * (ol[i] + orr[i]);
        ringPos = (ringPos + 1) % n;
        if (++samplesSinceFft >= n / 2)
        {
            samplesSinceFft = 0;
            for (int which = 0; which < 2; ++which)
            {
                const auto& ring = which == 0 ? ringIn : ringOut;
                auto& spec = which == 0 ? specIn : specOut;
                for (int k = 0; k < n; ++k)
                    fftWork[(size_t) k] = { (double) ring[(size_t) ((ringPos + k) % n)] * window[(size_t) k], 0.0 };
                fft.perform (fftWork);
                const double binHz = sampleRate / n;
                for (int p = 0; p < kDisplayPoints; ++p)
                {
                    // average the bins of a 1/24-octave slice (at least one bin), pink-referenced (+3 dB/oct)
                    const double f = displayFreq (p);
                    const double lo = f * std::pow (2.0, -1.0 / 48.0), hi = f * std::pow (2.0, 1.0 / 48.0);
                    int b0 = (int) std::floor (lo / binHz), b1 = (int) std::ceil (hi / binHz);
                    b0 = juce::jlimit (1, n / 2 - 1, b0); b1 = juce::jlimit (b0, n / 2 - 1, b1);
                    double e = 0.0;
                    for (int b = b0; b <= b1; ++b) e += std::norm (fftWork[(size_t) b]);
                    e /= (double) (b1 - b0 + 1);
                    const double db = 10.0 * std::log10 (e * 4.0 / ((double) n * n * 0.375) + 1e-14) + 10.0 * std::log10 (f / 1000.0);
                    const float prev = spec[(size_t) p];
                    spec[(size_t) p] = (float) (db > prev ? 0.5 * (db + prev) : prev + 0.15 * (db - prev));
                }
            }
        }
    }

    // LEARN capture
    if (state.load() == State::Capturing)
    {
        const int count = (int) juce::jmin ((size_t) numFrames, capMax - capLen);
        for (int i = 0; i < count; ++i)
        {
            capIn[0][capLen + (size_t) i] = il[i];
            if (capIn.size() > 1) capIn[1][capLen + (size_t) i] = ir[i];
            capSc[0][capLen + (size_t) i] = sl[i];
            capSc[1][capLen + (size_t) i] = sr2[i];
            const double m = 0.5 * ((double) il[i] + ir[i]);
            capBlockEnergy += m * m;
            if (++capBlockCount >= (int) (0.1 * sampleRate))
            {
                if (capBlockEnergy / capBlockCount > 1.0e-5)   // above about -50 dBFS RMS
                    capActiveSeconds += capBlockCount / sampleRate;
                capBlockEnergy = 0.0; capBlockCount = 0;
            }
        }
        capHasSc = capHasSc || scPeak > 1.0e-4f;
        capLen += (size_t) count;
        capturedActive = capActiveSeconds;
        captureProgress = (float) juce::jlimit (0.0, 1.0, capActiveSeconds / settings.captureSeconds);
        if (capActiveSeconds >= settings.captureSeconds || capLen >= capMax)
            launchAnalysis();
    }
}

//==============================================================================
Target AnalysisEngine::makeTarget (const AssistSettings& s) const
{
    if (! isVocalContext (s.context))
        return (s.genre < 0 && reference != nullptr) ? referenceTarget (*reference, referenceName.toStdString())
                                                     : mixTarget ((Genre) juce::jmax (0, s.genre));
    if (s.genre < 0 && reference != nullptr)
        return referenceTarget (*reference, referenceName.toStdString());
    return genreTarget ((Genre) juce::jlimit (0, (int) Genre::kNumGenres - 1, s.genre));
}

void AnalysisEngine::startLearning (const AssistSettings& s, std::function<sx::ParamValues()> provider)
{
    if (state.load() == State::Analysing) return;
    const juce::ScopedLock sl (lock);
    settings = s;
    settings.captureSeconds = juce::jlimit (5.0, 60.0, s.captureSeconds);
    paramsFn = std::move (provider);
    // allow up to 3x the requested signal time in wall-clock audio (pauses between takes)
    capMax = (size_t) (sampleRate * juce::jmin (180.0, settings.captureSeconds * 3.0));
    capIn.assign ((size_t) mainChannels, std::vector<float> (capMax, 0.0f));
    capSc.assign (2, std::vector<float> (capMax, 0.0f));
    capLen = 0; capHasSc = false; capActiveSeconds = 0.0; capBlockEnergy = 0.0; capBlockCount = 0;
    captureProgress = 0.0f; capturedActive = 0.0;
    state = State::Capturing;
}

void AnalysisEngine::finishLearningNow()
{
    const juce::ScopedLock sl (lock);
    if (state.load() == State::Capturing) launchAnalysis();
}

void AnalysisEngine::cancelLearning()
{
    const juce::ScopedLock sl (lock);
    if (state.load() == State::Capturing) state = report != nullptr ? State::Ready : State::Idle;
}

bool AnalysisEngine::hasCapture() const
{
    const juce::ScopedLock sl (lock);
    return capLen > (size_t) sampleRate && state.load() != State::Capturing;
}

bool AnalysisEngine::reanalyse (const AssistSettings& s, std::function<sx::ParamValues()> provider)
{
    const juce::ScopedLock sl (lock);
    if (state.load() == State::Capturing || state.load() == State::Analysing || capLen < (size_t) sampleRate) return false;
    settings = s;
    paramsFn = std::move (provider);
    launchAnalysis();
    return true;
}

void AnalysisEngine::launchAnalysis()
{
    // called with `lock` held
    state = State::Analysing;
    analysisProgress = 0.0f;
    auto raw = std::make_shared<Buffer> ();
    for (auto& ch : capIn) raw->emplace_back (ch.begin(), ch.begin() + (ptrdiff_t) capLen);
    std::shared_ptr<Buffer> sc;
    if (capHasSc)
    {
        sc = std::make_shared<Buffer>();
        for (auto& ch : capSc) sc->emplace_back (ch.begin(), ch.begin() + (ptrdiff_t) capLen);
    }
    AdvisorInput in;
    in.sampleRate = sampleRate;
    in.context = settings.context;
    in.target = makeTarget (settings);
    in.current = paramsFn ? paramsFn() : sx::defaultParamValues();
    in.cancel = &cancelFlag;
    in.progress = [this] (float p) { analysisProgress = p; };

    pool.addJob ([this, raw, sc, in]() mutable
    {
        in.raw = raw.get();
        in.sidechain = sc.get();
        auto rep = std::make_shared<Report> (Advisor (in).run());
        {
            const juce::ScopedLock sl2 (lock);
            report = rep;
        }
        ++reportVersion;
        state = State::Ready;
        analysisProgress = 1.0f;
    });
}

std::shared_ptr<const Report> AnalysisEngine::getReport() const
{
    const juce::ScopedLock sl (lock);
    return report;
}

//==============================================================================
void AnalysisEngine::loadReference (const juce::File& f)
{
    {
        const juce::ScopedLock sl (lock);
        referenceStatus = "Analysing " + f.getFileName() + "...";
        referenceFile = f;
    }
    pool.addJob ([this, f]
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
        if (reader == nullptr)
        {
            const juce::ScopedLock sl (lock);
            referenceStatus = "Could not read " + f.getFileName() + " (WAV, AIFF, FLAC, OGG or MP3 expected)";
            return;
        }
        const int chs = (int) juce::jmin (2u, reader->numChannels);
        const auto len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 180.0));
        juce::AudioBuffer<float> buf (chs, len);
        reader->read (&buf, 0, len, 0, true, chs > 1);
        Buffer b;
        for (int c = 0; c < chs; ++c) b.emplace_back (buf.getReadPointer (c), buf.getReadPointer (c) + len);
        auto m = std::make_shared<Measurements> (analyse (b, reader->sampleRate, AnalysisOptions {}));
        const juce::ScopedLock sl (lock);
        if (! m->valid || m->spectralFrames < 10)
        {
            referenceStatus = f.getFileName() + ": not enough signal to analyse";
            return;
        }
        reference = m;
        referenceName = f.getFileNameWithoutExtension();
        referenceStatus = juce::String::formatted ("%.1f LUFS, %.0f s analysed", m->integratedLufs, m->activeSeconds);
    });
}

void AnalysisEngine::clearReference()
{
    const juce::ScopedLock sl (lock);
    reference.reset();
    referenceName = {};
    referenceStatus = {};
    referenceFile = juce::File();
}

bool AnalysisEngine::hasReference() const { const juce::ScopedLock sl (lock); return reference != nullptr; }
juce::String AnalysisEngine::getReferenceName() const { const juce::ScopedLock sl (lock); return referenceName; }
juce::String AnalysisEngine::getReferenceStatus() const { const juce::ScopedLock sl (lock); return referenceStatus; }
juce::File AnalysisEngine::getReferenceFile() const { const juce::ScopedLock sl (lock); return referenceFile; }
std::shared_ptr<const Measurements> AnalysisEngine::getReferenceMeasurements() const { const juce::ScopedLock sl (lock); return reference; }

bool AnalysisEngine::waitForJobs (int timeoutMs)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (pool.getNumJobs() > 0)
    {
        if (juce::Time::getMillisecondCounter() > end) return false;
        juce::Thread::sleep (10);
    }
    return true;
}
