#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::String bandId(int band, const juce::String& suffix) { return "b" + juce::String(band+1) + "_" + suffix; }

RBTEQAudioProcessor::RBTEQAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    updateFilters();
}

juce::AudioProcessorValueTreeState::ParameterLayout RBTEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const char* names[8] = {"LOW", "LOW 2", "LOW MID", "MID", "MID 2", "HIGH MID", "HIGH", "AIR"};
    const float defaults[8] = {50,120,350,900,2200,4800,9000,15000};
    for (int i=0;i<8;++i) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(bandId(i,"freq"), "Band "+juce::String(i+1)+" Frequency", juce::NormalisableRange<float>(20.0f,20000.0f,0.01f,0.25f), defaults[i]));
        layout.add(std::make_unique<juce::AudioParameterFloat>(bandId(i,"gain"), "Band "+juce::String(i+1)+" Gain", juce::NormalisableRange<float>(-18.0f,18.0f,0.01f), 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(bandId(i,"q"), "Band "+juce::String(i+1)+" Q", juce::NormalisableRange<float>(0.1f,10.0f,0.001f,0.35f), 1.0f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(bandId(i,"type"), "Band "+juce::String(i+1)+" Type", juce::StringArray{"Bell","Low Shelf","High Shelf","HPF","LPF"}, 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(bandId(i,"slope"), "Band "+juce::String(i+1)+" Slope", juce::StringArray{"6 dB/oct","12 dB/oct","18 dB/oct","24 dB/oct","36 dB/oct","48 dB/oct"}, 3));
        layout.add(std::make_unique<juce::AudioParameterBool>(bandId(i,"on"), "Band "+juce::String(i+1)+" On", true));
        layout.add(std::make_unique<juce::AudioParameterBool>(bandId(i,"solo"), "Band "+juce::String(i+1)+" Solo", false));
    }
    return layout;
}

void RBTEQAudioProcessor::prepareToPlay(double sampleRate, int)
{
    sr=sampleRate; fifoIndex=0; fifo.fill(0.0f); fftData.fill(0.0f);
    for (auto& band:filters) for(auto& f:band) f.clear();
    for (auto& band:soloFilters) for(auto& f:band) f.clear();
    updateFilters();
}
void RBTEQAudioProcessor::releaseResources() {}
bool RBTEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const { return l.getMainInputChannelSet()==juce::AudioChannelSet::stereo() && l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo(); }

RBTEQAudioProcessor::Coeff RBTEQAudioProcessor::makeCoeff(int type,double f,double gain,double q,double sampleRate)
{
    f=juce::jlimit(20.0,sampleRate*0.48,f); q=juce::jlimit(0.1,10.0,q);
    const double A=std::pow(10.0,gain/40.0), w=2.0*juce::MathConstants<double>::pi*f/sampleRate, cw=std::cos(w), sw=std::sin(w), alpha=sw/(2.0*q), sqrtA=std::sqrt(A);
    Coeff c;
    switch(type) {
    case 1: { // low shelf
        const double al=sw/2.0*std::sqrt(2.0); const double beta=2.0*sqrtA*al;
        c.b0=A*((A+1)-(A-1)*cw+beta); c.b1=2*A*((A-1)-(A+1)*cw); c.b2=A*((A+1)-(A-1)*cw-beta);
        c.a0=(A+1)+(A-1)*cw+beta; c.a1=-2*((A-1)+(A+1)*cw); c.a2=(A+1)+(A-1)*cw-beta; break; }
    case 2: { // high shelf
        const double al=sw/2.0*std::sqrt(2.0); const double beta=2.0*sqrtA*al;
        c.b0=A*((A+1)+(A-1)*cw+beta); c.b1=-2*A*((A-1)+(A+1)*cw); c.b2=A*((A+1)+(A-1)*cw-beta);
        c.a0=(A+1)-(A-1)*cw+beta; c.a1=2*((A-1)-(A+1)*cw); c.a2=(A+1)-(A-1)*cw-beta; break; }
    case 3: // High-pass (HPF)
        c.b0=(1-cw)/2; c.b1=1-cw; c.b2=(1-cw)/2; c.a0=1+alpha; c.a1=-2*cw; c.a2=1-alpha; break;
    case 4: // Low-pass (LPF)
        c.b0=(1+cw)/2; c.b1=-(1+cw); c.b2=(1+cw)/2; c.a0=1+alpha; c.a1=-2*cw; c.a2=1-alpha; break;
    default: c.b0=1+alpha*A; c.b1=-2*cw; c.b2=1-alpha*A; c.a0=1+alpha/A; c.a1=-2*cw; c.a2=1-alpha/A; break;
    }
    return c;
}
RBTEQAudioProcessor::Coeff RBTEQAudioProcessor::makeBandPass(double f,double q,double sampleRate)
{
    const double w=2.0*juce::MathConstants<double>::pi*juce::jlimit(20.0,sampleRate*0.48,f)/sampleRate, alpha=std::sin(w)/(2.0*juce::jlimit(0.1,10.0,q)); Coeff c;
    c.b0=alpha; c.b1=0; c.b2=-alpha; c.a0=1+alpha; c.a1=-2*std::cos(w); c.a2=1-alpha; return c;
}
void RBTEQAudioProcessor::updateFilters()
{
    const double butterworthQ[4][4]={{0.70710678,0,0,0},{0.5411961,0.70710678,0,0},{0.51763809,0.70710678,1.0,0},{0.50979558,0.60134489,0.89997622,2.56291545}};
    for(int b=0;b<8;++b) {
        auto* fp=parameters.getRawParameterValue(bandId(b,"freq")); auto* gp=parameters.getRawParameterValue(bandId(b,"gain")); auto* qp=parameters.getRawParameterValue(bandId(b,"q")); auto* tp=parameters.getRawParameterValue(bandId(b,"type")); auto* sp=parameters.getRawParameterValue(bandId(b,"slope"));
        const double f=fp?fp->load():1000, g=gp?gp->load():0, q=qp?qp->load():1; const int t=tp?(int)tp->load():0, slope=sp?(int)sp->load():3;
        const int sections=(slope==0||slope==1)?1:(slope==2||slope==3)?2:slope==4?3:4; // section count for requested order
        const bool firstOrder=(slope==0||slope==2||slope==4);
        Coeff c=makeCoeff(t,f,g,q,sr), sc=makeBandPass(f,q,sr);
        for(int ch=0;ch<2;++ch) {
            auto& solo=soloFilters[b][ch]; solo.b0=sc.b0/sc.a0; solo.b1=sc.b1/sc.a0; solo.b2=sc.b2/sc.a0; solo.a1=sc.a1/sc.a0; solo.a2=sc.a2/sc.a0;
            for(int stage=0;stage<4;++stage){Coeff cc=c;
                if(t==3||t==4){ if(stage>=sections){cc={};} else {double qs= firstOrder&&stage==0?0.70710678:butterworthQ[sections-1][stage]; cc=makeCoeff(t,f,0.0,qs,sr); if(firstOrder&&stage==0){double w=2.0*juce::MathConstants<double>::pi*f/sr, k=std::tan(w/2.0); cc={}; if(t==3){cc.b0=1/(1+k);cc.b1=-1/(1+k);cc.b2=0;cc.a0=1;cc.a1=(1-k)/(1+k);cc.a2=0;}else{cc.b0=k/(1+k);cc.b1=k/(1+k);cc.b2=0;cc.a0=1;cc.a1=(1-k)/(1+k);cc.a2=0;}} }}
                auto& x=filters[b][stage][ch]; x.b0=cc.b0/cc.a0; x.b1=cc.b1/cc.a0; x.b2=cc.b2/cc.a0; x.a1=cc.a1/cc.a0; x.a2=cc.a2/cc.a0;
            }
        }
        activeSolo[b]=parameters.getRawParameterValue(bandId(b,"solo"))->load()>0.5f;
    }
}
void RBTEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n=buffer.getNumSamples();
    auto* L=buffer.getWritePointer(0); auto* R=buffer.getWritePointer(1);
    updateFilters();
    int solo=-1; for(int b=0;b<8;++b) if(activeSolo[b]) {solo=b;break;}
    for(int i=0;i<n;++i) {
        const float mono=(L[i]+R[i])*0.5f;
        fifo[(size_t)fifoIndex++]=mono;
        if(fifoIndex>=fftSize) {
            std::copy(fifo.begin(),fifo.end(),fftData.begin()); window.multiplyWithWindowingTable(fftData.data(),fftSize); fft.performFrequencyOnlyForwardTransform(fftData.data());
            std::array<float,512> temp{}; for(int k=0;k<512;++k) { const float mag=fftData[(size_t)k]/(fftSize*0.5f); temp[k]=juce::jlimit(-100.0f,0.0f,20.0f*std::log10(mag+1.0e-7f)); }
            if(spectrumLock.tryEnter()) { spectrum=temp; spectrumLock.exit(); }
            fifoIndex=0;
        }
        for(int ch=0;ch<2;++ch) {
            double x=ch==0?L[i]:R[i];
            if(solo>=0) x=soloFilters[solo][ch].process(x);
            else { for(int b=0;b<8;++b) if(parameters.getRawParameterValue(bandId(b,"on"))->load()>0.5f) for(int stage=0;stage<4;++stage) x=filters[b][stage][ch].process(x); }
            if(ch==0)L[i]=(float)x; else R[i]=(float)x;
        }
    }
}
void RBTEQAudioProcessor::getSpectrum(std::array<float,512>& out) const { if(spectrumLock.tryEnter()){out=spectrum;spectrumLock.exit();} }
float RBTEQAudioProcessor::getBandFrequency(int b) const{return parameters.getRawParameterValue(bandId(b,"freq"))->load();}
float RBTEQAudioProcessor::getBandGain(int b) const{return parameters.getRawParameterValue(bandId(b,"gain"))->load();}
float RBTEQAudioProcessor::getBandQ(int b) const{return parameters.getRawParameterValue(bandId(b,"q"))->load();}
int RBTEQAudioProcessor::getBandType(int b) const{return (int)parameters.getRawParameterValue(bandId(b,"type"))->load();}
bool RBTEQAudioProcessor::getBandEnabled(int b) const{return parameters.getRawParameterValue(bandId(b,"on"))->load()>0.5f;}
bool RBTEQAudioProcessor::getBandSolo(int b) const{return parameters.getRawParameterValue(bandId(b,"solo"))->load()>0.5f;}
void RBTEQAudioProcessor::setBandFrequency(int b,float v){auto* p=dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(bandId(b,"freq"))); if(p)p->setValueNotifyingHost(p->convertTo0to1(v));}
void RBTEQAudioProcessor::setBandGain(int b,float v){auto* p=dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(bandId(b,"gain"))); if(p)p->setValueNotifyingHost(p->convertTo0to1(v));}
void RBTEQAudioProcessor::getStateInformation(juce::MemoryBlock& dest){auto state=parameters.copyState(); std::unique_ptr<juce::XmlElement> xml(state.createXml()); copyXmlToBinary(*xml,dest);}
void RBTEQAudioProcessor::setStateInformation(const void* data,int size){std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data,size));if(xml&&xml->hasTagName(parameters.state.getType()))parameters.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessorEditor* RBTEQAudioProcessor::createEditor(){return new RBTEQAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new RBTEQAudioProcessor();}
