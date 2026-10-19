#pragma once
// Bridge between the real-time audio thread and the (non-real-time) analysis code.
//
//  audio thread --(lock-free FIFO of 6-channel frames: in L/R, out L/R, sidechain L/R)--> worker
//  worker thread: live meters (BS.1770 loudness in/out, true peak, correlation, display spectra)
//                 and LEARN capture of the raw input (+ sidechain)
//  pool job:      Advisor run on a finished capture, reference-file analysis
//
// The audio thread only ever calls push(): no locks, no allocation. Everything else is guarded by
// a CriticalSection that the audio thread never touches.

#include <JuceHeader.h>
#include "analysis/Advisor.h"

class AnalysisEngine : private juce::Thread
{
public:
    static constexpr int kFrameChannels = 6;
    static constexpr int kFifoFrames = 1 << 17;
    static constexpr int kDisplayPoints = 240;

    AnalysisEngine();
    ~AnalysisEngine() override;

    //== audio thread =========================================================
    /** Push one block. sc may be nullptr. All pointers: numSamples floats. */
    void push (const float* inL, const float* inR, const float* outL, const float* outR,
               const float* scL, const float* scR, int numSamples) noexcept;

    //== any non-audio thread =================================================
    void prepare (double sampleRate, int mainChannels);

    struct LiveMeters
    {
        double inMomentary = -200, inShortTerm = -200, inIntegrated = -200;
        double outMomentary = -200, outShortTerm = -200, outIntegrated = -200, outLra = 0;
        double outTruePeak = -200, correlation = 1.0, scShortTerm = -200;
        bool sidechainActive = false;
        long long droppedFrames = 0;
    };
    LiveMeters getLiveMeters() const;
    void resetIntegrated();

    /** Display spectra (dB, 1/24-octave points from 20 Hz to 20 kHz). */
    void getDisplaySpectra (std::array<float, kDisplayPoints>& in, std::array<float, kDisplayPoints>& out) const;
    static float displayFreq (int i) noexcept { return 20.0f * std::pow (1000.0f, (float) i / (float) (kDisplayPoints - 1)); }

    //== LEARN / analysis =====================================================
    enum class State { Idle, Capturing, Analysing, Ready };
    State getState() const noexcept { return state.load(); }
    float getCaptureProgress() const noexcept { return captureProgress.load(); }
    float getAnalysisProgress() const noexcept { return analysisProgress.load(); }
    double getCapturedActiveSeconds() const noexcept { return capturedActive.load(); }

    struct AssistSettings
    {
        sx::analysis::Context context = sx::analysis::Context::IsolatedVocal;
        int genre = 0;                     // index into genreNames(); -1 = reference
        double captureSeconds = 20.0;
    };

    /** Starts capturing; analysis runs automatically when enough signal has been captured. */
    void startLearning (const AssistSettings& s, std::function<sx::ParamValues()> paramsProvider);
    /** Stops capturing early and analyses what was captured so far (needs >= 3 s of signal). */
    void finishLearningNow();
    void cancelLearning();
    /** Re-runs the advisor on the last capture (e.g. after changing target or settings). */
    bool reanalyse (const AssistSettings& s, std::function<sx::ParamValues()> paramsProvider);
    bool hasCapture() const;

    std::shared_ptr<const sx::analysis::Report> getReport() const;
    int getReportVersion() const noexcept { return reportVersion.load(); }

    //== reference ============================================================
    void loadReference (const juce::File& f);
    void clearReference();
    bool hasReference() const;
    juce::String getReferenceName() const;
    juce::String getReferenceStatus() const;
    juce::File getReferenceFile() const;
    std::shared_ptr<const sx::analysis::Measurements> getReferenceMeasurements() const;

    /** Blocks until pending pool jobs finish (tests / shutdown). */
    bool waitForJobs (int timeoutMs);

private:
    void run() override;
    void processChunk (const float* frames, int numFrames);
    void launchAnalysis();
    sx::analysis::Target makeTarget (const AssistSettings& s) const;

    // FIFO (written by the audio thread only)
    juce::AbstractFifo fifo { kFifoFrames };
    std::vector<float> fifoData;
    std::atomic<long long> dropped { 0 };

    mutable juce::CriticalSection lock;
    double sampleRate = 48000.0;
    int mainChannels = 2;

    // live meters (worker thread, under lock)
    sx::analysis::LoudnessMeter inLm, outLm, scLm;
    sx::analysis::TruePeakMeter outTp;
    double corrLL = 0, corrRR = 0, corrLR = 0;
    int corrCount = 0;
    double lastCorrelation = 1.0;
    int scSilentFrames = 1 << 30;

    // display spectra
    sx::analysis::Fft fft { 12 };
    std::vector<float> ringIn, ringOut;
    int ringPos = 0, samplesSinceFft = 0;
    std::vector<std::complex<double>> fftWork;
    std::vector<double> window;
    std::array<float, kDisplayPoints> specIn {}, specOut {};

    // capture
    std::atomic<State> state { State::Idle };
    std::atomic<float> captureProgress { 0.0f }, analysisProgress { 0.0f };
    std::atomic<double> capturedActive { 0.0 };
    sx::analysis::Buffer capIn, capSc;
    size_t capLen = 0, capMax = 0;
    bool capHasSc = false;
    double capActiveSeconds = 0.0, capBlockEnergy = 0.0;
    int capBlockCount = 0;
    AssistSettings settings;
    std::function<sx::ParamValues()> paramsFn;

    std::shared_ptr<const sx::analysis::Report> report;
    std::atomic<int> reportVersion { 0 };

    // reference
    std::shared_ptr<const sx::analysis::Measurements> reference;
    juce::String referenceName, referenceStatus;
    juce::File referenceFile;

    juce::ThreadPool pool { 1 };
    std::atomic<bool> cancelFlag { false };
    std::vector<float> chunk;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisEngine)
};
