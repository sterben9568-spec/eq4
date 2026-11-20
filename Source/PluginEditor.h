#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class RBTEQAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RBTEQAudioProcessorEditor(RBTEQAudioProcessor&);
    ~RBTEQAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    class Analyzer : public juce::Component {
    public:
        Analyzer(RBTEQAudioProcessorEditor& e):owner(e){}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        int hitBand(juce::Point<float>) const;
    private: RBTEQAudioProcessorEditor& owner;
    };
    class BandStrip : public juce::Component {
    public:
        BandStrip(RBTEQAudioProcessorEditor&,int);
        void paint(juce::Graphics&) override;
        void resized() override;
        int band;
        juce::Slider freq,gain,q;
        juce::ComboBox type, slope;
        juce::ToggleButton on,solo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fa,ga,qa;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ta, sla;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> oa,sa;
        RBTEQAudioProcessorEditor& owner;
    };
    void timerCallback() override;
    void syncGraph();
    RBTEQAudioProcessor& processor;
    Analyzer analyzer;
    std::array<std::unique_ptr<BandStrip>,8> strips;
    juce::Label logo, subtitle, shiftLabel;
    bool shiftHeld = false;
    std::array<float,512> spectrum{};
    int selectedBand=0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RBTEQAudioProcessorEditor)
};
