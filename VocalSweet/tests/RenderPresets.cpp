// Offline renderer: writes test WAV files (synthetic vocal + standard test signals) and the
// result of processing them through every factory preset. Usage: VocalSweetRender <outDir>
//
// NOTE: the "vocal" is a synthetic, formant-filtered test signal, not a recording of a singer.

#include "TestUtils.h"
#include <filesystem>

using namespace vt;
namespace fs = std::filesystem;

static std::string slug (std::string s)
{
    for (auto& c : s) c = std::isalnum ((unsigned char) c) ? (char) std::tolower ((unsigned char) c) : '_';
    return s;
}

int main (int argc, char** argv)
{
    const std::string out = argc > 1 ? argv[1] : "audio";
    fs::create_directories (out + "/input");
    fs::create_directories (out + "/processed");
    const int sr = 48000;

    auto vocal = syntheticVocal (sr, 8.0, 2, 0.3f);
    writeWav (out + "/input/synthetic_vocal_dry.wav", vocal, sr);

    // standard test signals
    auto sweep = makeBuffer (2, sr * 6);
    for (size_t i = 0; i < sweep[0].size(); ++i)
    {
        const double t = (double) i / sr, T = 6.0, f0 = 20.0, f1 = 20000.0;
        const double ph = 2.0 * vs::dsp::kPi * f0 * T / std::log (f1 / f0) * (std::exp (t / T * std::log (f1 / f0)) - 1.0);
        sweep[0][i] = sweep[1][i] = 0.25f * (float) std::sin (ph);
    }
    writeWav (out + "/input/log_sweep_20Hz-20kHz_-12dBFS.wav", sweep, sr);
    auto pink = makeBuffer (2, sr * 4);
    pinkNoise (pink, 1.0f);
    writeWav (out + "/input/pink_noise.wav", pink, sr);

    std::FILE* csv = std::fopen ((out + "/render_summary.csv").c_str(), "w");
    std::fprintf (csv, "file,preset,input_peak_dBFS,input_rms_dBFS,output_peak_dBFS,output_rms_dBFS\n");

    auto render = [&] (const Buffer& in, const vs::ParamValues& v, const std::string& name, const std::string& presetName)
    {
        vs::dsp::VocalChain c;
        prepareWith (c, sr, v);
        auto b = in;
        processChain (c, b, 512);
        const std::string path = out + "/processed/" + name + ".wav";
        writeWav (path, b, sr);
        std::fprintf (csv, "%s,%s,%.2f,%.2f,%.2f,%.2f\n", (name + ".wav").c_str(), presetName.c_str(),
                      toDb (peak (in)), toDb (rms (in[0])), toDb (peak (b)), toDb (rms (b[0])));
        std::printf ("wrote %s\n", path.c_str());
    };

    for (const auto& p : vs::factoryPresets())
        render (vocal, p.values, "vocal__" + slug (p.name), p.name);

    for (int s : { 0, 50, 100 })
    {
        auto v = vs::defaultParamValues(); v[vs::pSweeten] = (float) s;
        render (vocal, v, "vocal__default_sweeten_" + std::to_string (s), "Default, Sweeten " + std::to_string (s));
    }
    render (sweep, vs::factoryPresets()[1].values, "sweep__afrobeat_lead", "Afrobeat Lead");
    render (pink, vs::factoryPresets()[4].values, "pink__bright_pop_vocal", "Bright Pop Vocal");

    std::fclose (csv);
    return 0;
}
