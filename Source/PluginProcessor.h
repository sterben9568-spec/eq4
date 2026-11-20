#pragma once
#include <JuceHeader.h>

class RBTEQAudioProcessor : public juce::AudioProcessor
{
public:
    RBTEQAudioProcessor();
    ~RBTEQAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "RBT EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void getSpectrum(std::array<float, 512>& out) const;
    float getBandFrequency(int band) const;
    float getBandGain(int band) const;
    float getBandQ(int band) const;
    int getBandType(int band) const;
    bool getBandEnabled(int band) const;
    bool getBandSolo(int band) const;
    void setBandFrequency(int band, float value);
    void setBandGain(int band, float value);

private:
    struct Biquad { double b0=1,b1=0,b2=0,a1=0,a2=0; double z1=0,z2=0; double process(double x) { const double y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y; return y; } void clear(){z1=z2=0;} };
    struct Coeff { double b0=1,b1=0,b2=0,a0=1,a1=0,a2=0; };
    static Coeff makeCoeff(int type, double f, double gain, double q, double sr);
    static Coeff makeBandPass(double f, double q, double sr);
    void updateFilters();
    std::array<std::array<std::array<Biquad,2>,4>,8> filters;
    std::array<std::array<Biquad,2>,8> soloFilters;
    std::array<bool,8> activeSolo{};
    double sr=44100.0;

    static constexpr int fftOrder=10;
    static constexpr int fftSize=1<<fftOrder;
    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> window{fftSize, juce::dsp::WindowingFunction<float>::hann, false};
    std::array<float, fftSize*2> fftData{};
    std::array<float, fftSize> fifo{};
    int fifoIndex=0;
    mutable juce::SpinLock spectrumLock;
    std::array<float,512> spectrum{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RBTEQAudioProcessor)
};
