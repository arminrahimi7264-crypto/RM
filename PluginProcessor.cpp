#include "PluginProcessor.h"
using namespace juce;

namespace
{
float blep (float t, float dt)
{
    if (t < dt)        { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt)  { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

float osc (int w, float ph, float dt)
{
    switch (w)
    {
        case 0:  return std::sin (MathConstants<float>::twoPi * ph);
        case 1:  return 4.f * std::abs (ph - .5f) - 1.f;
        case 2:  return 2.f * ph - 1.f - blep (ph, dt);
        default: { float p2 = ph + .5f; if (p2 >= 1.f) p2 -= 1.f;
                   return (ph < .5f ? 1.f : -1.f) + blep (ph, dt) - blep (p2, dt); }
    }
}

struct Sound : SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

struct Voice : SynthesiserVoice
{
    explicit Voice (AudioProcessorValueTreeState& a) : s (a) {}
    AudioProcessorValueTreeState& s;
    float ph[2][5] {};
    float hz = 440, vel = 1, bend = 0, lfoPh = 0;
    double sr = 44100;
    ADSR amp, fe;
    dsp::StateVariableTPTFilter<float> flt;

    float P (const char* id) { return s.getRawParameterValue (id)->load(); }
    bool canPlaySound (SynthesiserSound* x) override { return dynamic_cast<Sound*> (x) != nullptr; }
    void pitchWheelMoved (int v) override { bend = (v - 8192) / 8192.f * 2.f; }
    void controllerMoved (int, int) override {}

    void startNote (int n, float v, SynthesiserSound*, int pw) override
    {
        sr = getSampleRate();
        hz = (float) MidiMessage::getMidiNoteInHertz (n);
        vel = v;
        bend = (pw - 8192) / 8192.f * 2.f;
        Random r;
        for (auto& a : ph) for (auto& b : a) b = r.nextFloat();
        ADSR::Parameters p { P ("a"), P ("d"), P ("s"), P ("r") };
        amp.setSampleRate (sr); fe.setSampleRate (sr);
        amp.setParameters (p);  fe.setParameters (p);
        amp.noteOn(); fe.noteOn();
        dsp::ProcessSpec sp; sp.sampleRate = sr; sp.maximumBlockSize = 512; sp.numChannels = 1;
        flt.prepare (sp); flt.reset();
    }

    void stopNote (float, bool tail) override
    {
        if (tail) { amp.noteOff(); fe.noteOff(); }
        else      { amp.reset(); clearCurrentNote(); }
    }

    void renderNextBlock (AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive()) return;
        const int un = (int) P ("un");
        const float sp = P ("sp") * 40.f, mix = P ("mix");
        const int w[2] = { (int) P ("o1w"), (int) P ("o2w") };
        const float oc[2] = { std::pow (2.f, P ("o1o")), std::pow (2.f, P ("o2o")) };
        const float dt[2] = { P ("o1d"), P ("o2d") };
        const float g[2] = { 1.f - mix * .5f, mix };
        const float cut = P ("cut"), fa = P ("fe") * 60.f, lr = P ("lr"), la = P ("la") * 2400.f;
        const float vs = P ("vs"), gain = .3f / std::sqrt ((float) un) * (1.f - vs + vs * vel);
        const int ft = (int) P ("ft");
        flt.setType (ft == 0 ? dsp::StateVariableTPTFilterType::lowpass
                   : ft == 1 ? dsp::StateVariableTPTFilterType::highpass
                             : dsp::StateVariableTPTFilterType::bandpass);
        flt.setResonance (P ("res"));

        float inc[2][5];
        for (int k = 0; k < 2; ++k)
            for (int u = 0; u < un; ++u)
            {
                float so = 0.f;
                if (u > 0) so = (u % 2 ? 1.f : -1.f) * std::ceil (u / 2.f) / std::ceil ((un - 1) / 2.f) * sp;
                inc[k][u] = hz * oc[k] * std::pow (2.f, (dt[k] + so + bend * 100.f) / 1200.f) / (float) sr;
            }

        for (int i = 0; i < num; ++i)
        {
            const float ae = amp.getNextSample(), fv = fe.getNextSample();
            if (! amp.isActive()) { clearCurrentNote(); break; }
            lfoPh += lr / (float) sr; if (lfoPh >= 1.f) lfoPh -= 1.f;
            if ((i & 7) == 0)
            {
                const float l = std::sin (MathConstants<float>::twoPi * lfoPh);
                const float c = cut * std::pow (2.f, (fa * fv + la * l) / 1200.f);
                flt.setCutoffFrequency (jlimit (20.f, (float) sr * .45f, c));
            }
            float x = 0.f;
            for (int k = 0; k < 2; ++k)
                for (int u = 0; u < un; ++u)
                {
                    ph[k][u] += inc[k][u]; if (ph[k][u] >= 1.f) ph[k][u] -= 1.f;
                    x += g[k] * osc (w[k], ph[k][u], inc[k][u]);
                }
            const float y = flt.processSample (0, x) * ae * gain;
            for (int ch = 0; ch < out.getNumChannels(); ++ch) out.addSample (ch, start + i, y);
        }
    }
};

AudioProcessorValueTreeState::ParameterLayout layout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto F = [&] (const char* id, const char* n, float lo, float hi, float def, float skew = 1.f)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, n, NormalisableRange<float> (lo, hi, 0.001f, skew), def)); };
    auto I = [&] (const char* id, const char* n, int lo, int hi, int def)
    { l.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, n, lo, hi, def)); };
    auto C = [&] (const char* id, const char* n, StringArray a, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, n, a, def)); };
    const StringArray W { "Sine", "Triangle", "Saw", "Square" };
    C ("o1w", "Osc1 Wave", W, 2); I ("o1o", "Osc1 Octave", -2, 2, 0); F ("o1d", "Osc1 Detune", -50, 50, 0);
    C ("o2w", "Osc2 Wave", W, 2); I ("o2o", "Osc2 Octave", -2, 2, 0); F ("o2d", "Osc2 Detune", -50, 50, 7);
    F ("mix", "Osc2 Mix", 0, 1, .5f);
    I ("un", "Unison Voices", 1, 5, 1); F ("sp", "Unison Spread", 0, 1, .3f);
    C ("ft", "Filter Type", { "Lowpass", "Highpass", "Bandpass" }, 0);
    F ("cut", "Cutoff", 20, 20000, 1500, .3f); F ("res", "Resonance", .5f, 12, .7f); F ("fe", "Filter Env", 0, 100, 20);
    F ("a", "Attack", .001f, 4, .01f, .4f); F ("d", "Decay", .001f, 4, .3f, .4f);
    F ("s", "Sustain", 0, 1, .8f); F ("r", "Release", .001f, 6, .3f, .4f);
    F ("lr", "LFO Rate", .1f, 12, 2); F ("la", "LFO to Cutoff", 0, 1, 0);
    F ("vs", "Velocity", 0, 1, .5f);
    F ("dm", "Delay Mix", 0, .9f, 0); F ("dt", "Delay Time", .05f, 1, .3f); F ("df", "Delay Feedback", 0, .9f, .3f);
    F ("rm", "Reverb Mix", 0, 1, .2f); F ("vol", "Volume", 0, 1, .6f);
    return l;
}

using PV = std::vector<std::pair<const char*, float>>;
struct Preset { const char* name; PV v; };
const std::vector<Preset> PRESETS = {
 { "Warm Pad", { {"o1d",-8},{"o2d",8},{"mix",.7f},{"un",4},{"sp",.5f},{"cut",520},{"res",1},{"fe",20},{"a",.9f},{"d",.75f},{"s",.8f},{"r",1.7f},{"lr",2.5f},{"la",.1f},{"vs",.3f},{"dm",.25f},{"dt",.4f},{"df",.3f},{"rm",.45f},{"vol",.6f} } },
 { "Pluck Lead", { {"o2w",3},{"o2d",6},{"mix",.4f},{"un",3},{"sp",.3f},{"cut",221},{"res",4},{"fe",70},{"a",.001f},{"d",.27f},{"s",.1f},{"r",.19f},{"vs",.7f},{"dm",.3f},{"dt",.375f},{"df",.35f},{"rm",.2f} } },
 { "Deep Bass", { {"o1o",-1},{"o2w",3},{"o2o",-2},{"o2d",0},{"mix",.5f},{"cut",221},{"res",2},{"fe",40},{"a",.001f},{"d",.27f},{"s",.8f},{"r",.07f},{"vs",.6f},{"dm",0},{"rm",0},{"vol",.7f} } },
 { "Strings", { {"o1d",-12},{"o2d",12},{"mix",.8f},{"un",5},{"sp",.45f},{"cut",920},{"res",.7f},{"fe",10},{"a",.61f},{"d",.75f},{"s",.85f},{"r",1.08f},{"lr",3.7f},{"la",.05f},{"vs",.4f},{"dm",.1f},{"dt",.3f},{"df",.2f},{"rm",.6f},{"vol",.55f} } },
 { "Bell Pluck", { {"o1w",1},{"o1o",1},{"o2w",0},{"o2o",2},{"o2d",7},{"mix",.5f},{"un",2},{"sp",.25f},{"cut",2170},{"res",1},{"fe",30},{"a",.001f},{"d",.37f},{"s",0},{"r",.37f},{"vs",.7f},{"dm",.4f},{"dt",.3f},{"df",.45f},{"rm",.3f},{"vol",.55f} } }
};
}

SynthProcessor::SynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", layout())
{
    for (int i = 0; i < 8; ++i) synth.addVoice (new Voice (apvts));
    synth.addSound (new Sound());
}

void SynthProcessor::prepareToPlay (double sr, int bs)
{
    synth.setCurrentPlaybackSampleRate (sr);
    dsp::ProcessSpec sp; sp.sampleRate = sr; sp.maximumBlockSize = (uint32) bs; sp.numChannels = 2;
    delay.prepare (sp); delay.reset();
    reverb.setSampleRate (sr); reverb.reset();
}

void SynthProcessor::processBlock (AudioBuffer<float>& b, MidiBuffer& m)
{
    ScopedNoDenormals nd;
    b.clear();
    const int n = b.getNumSamples();
    synth.renderNextBlock (b, m, 0, n);

    const float dm = P ("dm"), df = P ("df");
    delay.setDelay (jmin (190000.f, P ("dt") * (float) getSampleRate()));
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = b.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const float in = d[i], o = delay.popSample (ch);
            delay.pushSample (ch, in + o * df);
            d[i] = in + o * dm;
        }
    }
    Reverb::Parameters rp; rp.roomSize = .8f; rp.damping = .5f; rp.wetLevel = P ("rm") * .4f; rp.dryLevel = 1.f; rp.width = 1.f;
    reverb.setParameters (rp);
    reverb.processStereo (b.getWritePointer (0), b.getWritePointer (1), n);
    b.applyGain (P ("vol"));
}

int SynthProcessor::getNumPrograms() { return (int) PRESETS.size(); }
const String SynthProcessor::getProgramName (int i) { return PRESETS[(size_t) i].name; }

void SynthProcessor::setCurrentProgram (int i)
{
    prog = i;
    for (auto* p : getParameters()) p->setValueNotifyingHost (p->getDefaultValue());
    for (auto& kv : PRESETS[(size_t) i].v)
        if (auto* p = apvts.getParameter (kv.first)) p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
}

void SynthProcessor::getStateInformation (MemoryBlock& d)
{
    std::unique_ptr<XmlElement> xml (apvts.copyState().createXml());
    copyXmlToBinary (*xml, d);
}

void SynthProcessor::setStateInformation (const void* d, int s)
{
    std::unique_ptr<XmlElement> x (getXmlFromBinary (d, s));
    if (x != nullptr && x->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*x));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SynthProcessor(); }
