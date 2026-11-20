#include "PluginEditor.h"

namespace {
juce::String bandId(int band,const juce::String& suffix){return "b"+juce::String(band+1)+"_"+suffix;}
const juce::Colour bg=juce::Colour(0xff101214), panel=juce::Colour(0xff181b1e), grid=juce::Colour(0xff30363a), text=juce::Colour(0xffcbd1d4), muted=juce::Colour(0xff737d82);
const std::array<juce::Colour,8> colors={juce::Colour(0xffe5a64b),juce::Colour(0xffe08a56),juce::Colour(0xffd46b65),juce::Colour(0xffb67bd1),juce::Colour(0xff8d83df),juce::Colour(0xff5d9ed1),juce::Colour(0xff60b9a4),juce::Colour(0xff9abf65)};
float logX(float f,juce::Rectangle<float> r){return r.getX()+r.getWidth()*std::log10(juce::jlimit(20.0f,20000.0f,f)/20.0f)/3.0f;}
float dbY(float d,juce::Rectangle<float> r){return r.getCentreY()-d*(r.getHeight()/36.0f);}
void styleSlider(juce::Slider& s){s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);s.setMouseDragSensitivity(150);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,56,16);s.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xff5b6267));s.setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour(0xff343a3e));s.setColour(juce::Slider::thumbColourId,juce::Colour(0xffe3e7e8));s.setColour(juce::Slider::textBoxTextColourId,text);s.setColour(juce::Slider::textBoxBackgroundColourId,panel);s.setColour(juce::Slider::textBoxOutlineColourId,panel);}
}
RBTEQAudioProcessorEditor::RBTEQAudioProcessorEditor(RBTEQAudioProcessor& p):AudioProcessorEditor(&p),processor(p),analyzer(*this)
{
    setResizable(true,true);setResizeLimits(1050,500,1500,760);setSize(1260,560);
    logo.setText("RBT / EQ",juce::dontSendNotification);logo.setFont(juce::Font(21.0f,juce::Font::bold));logo.setColour(juce::Label::textColourId,juce::Colour(0xffe7e9ea));
    subtitle.setText("8 BAND STEREO EQUALIZER",juce::dontSendNotification);subtitle.setFont(juce::Font(10.0f));subtitle.setColour(juce::Label::textColourId,muted);
    addAndMakeVisible(logo);addAndMakeVisible(subtitle);addAndMakeVisible(analyzer);
    shiftLabel.setText("SHIFT  FINE",juce::dontSendNotification);shiftLabel.setFont(juce::Font(11.0f,juce::Font::bold));shiftLabel.setJustificationType(juce::Justification::centred);shiftLabel.setColour(juce::Label::textColourId,muted);shiftLabel.setColour(juce::Label::backgroundColourId,juce::Colour(0xff202428));shiftLabel.setColour(juce::Label::outlineColourId,grid);addAndMakeVisible(shiftLabel);
    for(int i=0;i<8;++i){strips[i]=std::make_unique<BandStrip>(*this,i);addAndMakeVisible(*strips[i]);}
    startTimerHz(30);
}
RBTEQAudioProcessorEditor::~RBTEQAudioProcessorEditor(){stopTimer();}
void RBTEQAudioProcessorEditor::paint(juce::Graphics& g){
    g.fillAll(bg);
    g.setColour(juce::Colour(0xff080a0b));g.drawLine(22,58,getWidth()-22,58,1.0f);
    g.setColour(juce::Colour(0xff2a2f32));g.drawLine(22,59,getWidth()-22,59,1.0f);
    g.setColour(muted);g.setFont(9.0f);g.drawText("EQ BANDS",24,66,180,14,juce::Justification::left);
    g.drawText("SPECTRUM / RESPONSE",getWidth()-286,66,260,14,juce::Justification::left);
    g.setColour(juce::Colour(0xff737d82));g.drawText("STEREO  •  NO M/S",24,getHeight()-22,200,12,juce::Justification::left);
    g.drawText("RBT AUDIO  /  PROTOTYPE 0.2",getWidth()-240,getHeight()-22,216,12,juce::Justification::right);
}
void RBTEQAudioProcessorEditor::resized(){
    logo.setBounds(24,13,145,34);subtitle.setBounds(180,22,240,20);shiftLabel.setBounds(getWidth()-126,17,102,27);
    const int graphW=258, margin=24, gap=14;
    analyzer.setBounds(getWidth()-margin-graphW,86,graphW,getHeight()-126);
    const int leftW=getWidth()-margin*2-graphW-gap;
    const int x=margin, y=86, h=getHeight()-126, w=leftW/8;
    for(int i=0;i<8;++i)strips[i]->setBounds(x+i*w,y,w,h);
}
void RBTEQAudioProcessorEditor::timerCallback(){processor.getSpectrum(spectrum);analyzer.repaint();
    const bool held=juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::shiftKey);
    if(held!=shiftHeld){shiftHeld=held;shiftLabel.setColour(juce::Label::textColourId,held?juce::Colour(0xffe5a64b):muted);shiftLabel.setColour(juce::Label::backgroundColourId,held?juce::Colour(0xff3a2c1d):juce::Colour(0xff202428));shiftLabel.repaint();}
    for(auto& strip:strips)if(strip){int sensitivity=held?1400:150;strip->freq.setMouseDragSensitivity(sensitivity);strip->gain.setMouseDragSensitivity(sensitivity);strip->q.setMouseDragSensitivity(sensitivity);}
void RBTEQAudioProcessorEditor::syncGraph(){analyzer.repaint();}

void RBTEQAudioProcessorEditor::Analyzer::paint(juce::Graphics& g)
{
    auto r=getLocalBounds().toFloat().reduced(1);g.setColour(panel);g.fillRoundedRectangle(r,7);g.setColour(grid);g.drawRoundedRectangle(r,7,1);
    auto plot=r.reduced(27,18); const float freqs[]={20,30,50,100,200,500,1000,2000,5000,10000,20000};
    for(float f:freqs){float x=logX(f,plot);g.setColour(grid.withAlpha(0.65f));g.drawVerticalLine((int)x,plot.getY(),plot.getBottom());g.setColour(muted);g.setFont(9);if(f==20||f==100||f==1000||f==10000||f==20000)g.drawText(f>=1000?juce::String(f/1000, f==1000?0:1)+"k":juce::String((int)f),x-16,plot.getBottom()+5,32,13,juce::Justification::centred);}
    for(int d=-18;d<=18;d+=12){float y=dbY((float)d,plot);g.setColour(grid.withAlpha(0.6f));g.drawHorizontalLine((int)y,plot.getX(),plot.getRight());g.setColour(muted);g.drawText((d>0?"+":"")+juce::String(d),1,y-7,23,14,juce::Justification::centredRight);}
    g.setColour(juce::Colour(0xff858d92));g.drawHorizontalLine((int)plot.getCentreY(),plot.getX(),plot.getRight());
    // FFT trace: map bins to log-frequency and draw a soft line
    juce::Path spec;bool started=false;for(int px=0;px<(int)plot.getWidth();++px){float f=20.0f*std::pow(1000.0f,(float)px/plot.getWidth());int bin=juce::jlimit(1,511,(int)(f*1024.0/owner.processor.getSampleRate()));float d=owner.spectrum[(size_t)bin];float x=plot.getX()+px,y=plot.getBottom()-juce::jmap(juce::jlimit(-90.0f,-10.0f,d),-90.0f,-10.0f,0.0f,plot.getHeight());if(!started){spec.startNewSubPath(x,y);started=true;}else spec.lineTo(x,y);}
    g.setColour(juce::Colour(0xff5a91a1).withAlpha(0.20f));juce::Path fill=spec;fill.lineTo(plot.getRight(),plot.getBottom());fill.lineTo(plot.getX(),plot.getBottom());fill.closeSubPath();g.fillPath(fill);g.setColour(juce::Colour(0xff79b3c1).withAlpha(0.75f));g.strokePath(spec,juce::PathStrokeType(1.2f));
    // summed EQ curve approximation
    juce::Path curve;for(int px=0;px<(int)plot.getWidth();++px){float f=20.0f*std::pow(1000.0f,(float)px/plot.getWidth());float db=0;for(int b=0;b<8;++b)if(owner.processor.getBandEnabled(b)){float fc=owner.processor.getBandFrequency(b),gain=owner.processor.getBandGain(b),q=owner.processor.getBandQ(b);float type=owner.processor.getBandType(b);if(type<=2){float oct=std::log2(f/fc);float width=1.0f/q;float response=gain*std::exp(-0.5f*(oct/width)*(oct/width));if(type==1)response*=1.0f/(1.0f+std::pow(f/fc,2));if(type==2)response*=1.0f/(1.0f+std::pow(fc/f,2));db+=response;}else if(type==3&&f<fc)db-=18.0f*std::log2(fc/f);else if(type==4&&f>fc)db-=18.0f*std::log2(f/fc);}
        float x=plot.getX()+px,y=dbY(juce::jlimit(-18.0f,18.0f,db),plot);if(px==0)curve.startNewSubPath(x,y);else curve.lineTo(x,y);}
    g.setColour(juce::Colour(0xffe6e8e9));g.strokePath(curve,juce::PathStrokeType(2.0f));
    for(int b=0;b<8;++b){float x=logX(owner.processor.getBandFrequency(b),plot),y=dbY(owner.processor.getBandGain(b),plot);g.setColour(colors[b]);g.fillEllipse(x-5,y-5,10,10);g.setColour(juce::Colour(0xff101214));g.drawEllipse(x-5,y-5,10,10,1.2f);if(b==owner.selectedBand){g.setColour(colors[b].withAlpha(0.3f));g.drawEllipse(x-10,y-10,20,20,1.2f);}}
}
int RBTEQAudioProcessorEditor::Analyzer::hitBand(juce::Point<float> p) const{auto plot=getLocalBounds().toFloat().reduced(27,18);int best=-1;float distance=14;for(int b=0;b<8;++b){float x=logX(owner.processor.getBandFrequency(b),plot),y=dbY(owner.processor.getBandGain(b),plot);float d=p.getDistanceFrom({x,y});if(d<distance){distance=d;best=b;}}return best;}
void RBTEQAudioProcessorEditor::Analyzer::mouseDown(const juce::MouseEvent& e){int b=hitBand(e.position);if(b>=0)owner.selectedBand=b;mouseDrag(e);}
void RBTEQAudioProcessorEditor::Analyzer::mouseDrag(const juce::MouseEvent& e){int b=owner.selectedBand;auto plot=getLocalBounds().toFloat().reduced(27,18);float f=20.0f*std::pow(1000.0f,juce::jlimit(0.0f,1.0f,(e.position.x-plot.getX())/plot.getWidth()));float gain=juce::jlimit(-18.0f,18.0f,(plot.getCentreY()-e.position.y)/(plot.getHeight()/36.0f));owner.processor.setBandFrequency(b,f);owner.processor.setBandGain(b,gain);owner.strips[b]->freq.setValue(f,juce::dontSendNotification);owner.strips[b]->gain.setValue(gain,juce::dontSendNotification);repaint();}

RBTEQAudioProcessorEditor::BandStrip::BandStrip(RBTEQAudioProcessorEditor& e,int b):band(b),owner(e)
{
    styleSlider(freq);styleSlider(gain);styleSlider(q);freq.setRange(20,20000,0.01);freq.setSkewFactorFromMidPoint(1000);freq.setNumDecimalPlacesToDisplay(0);freq.setTextValueSuffix(" Hz");gain.setRange(-18,18,0.01);gain.setTextValueSuffix(" dB");q.setRange(0.1,10,0.001);q.setTextValueSuffix(" Q");
    type.addItemList({"Bell","L-Shelf","H-Shelf","HPF","LPF"},1);slope.addItemList({"6 dB/oct","12 dB/oct","18 dB/oct","24 dB/oct","36 dB/oct","48 dB/oct"},1);slope.setColour(juce::ComboBox::backgroundColourId,panel);slope.setColour(juce::ComboBox::textColourId,text);slope.setColour(juce::ComboBox::outlineColourId,grid);type.setColour(juce::ComboBox::backgroundColourId,panel);type.setColour(juce::ComboBox::textColourId,text);type.setColour(juce::ComboBox::outlineColourId,grid);
    on.setButtonText("ON");solo.setButtonText("SOLO");on.setColour(juce::ToggleButton::textColourId,text);solo.setColour(juce::ToggleButton::textColourId,colors[b]);
    addAndMakeVisible(freq);addAndMakeVisible(gain);addAndMakeVisible(q);addAndMakeVisible(type);addAndMakeVisible(slope);addAndMakeVisible(on);addAndMakeVisible(solo);
    fa=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.processor.parameters,bandId(b,"freq"),freq);ga=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.processor.parameters,bandId(b,"gain"),gain);qa=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.processor.parameters,bandId(b,"q"),q);ta=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.processor.parameters,bandId(b,"type"),type);sla=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.processor.parameters,bandId(b,"slope"),slope);oa=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.processor.parameters,bandId(b,"on"),on);sa=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.processor.parameters,bandId(b,"solo"),solo);
}
void RBTEQAudioProcessorEditor::BandStrip::paint(juce::Graphics& g){
    auto r=getLocalBounds().toFloat();g.setColour(panel);g.fillRoundedRectangle(r.reduced(1),4);g.setColour(juce::Colour(0xff353b3e));g.drawRoundedRectangle(r.reduced(1),4,1.0f);
    g.setColour(colors[band]);g.fillRoundedRectangle(r.getX()+4,r.getY()+4,r.getWidth()-8,2.5f,1.2f);
    g.setColour(text);g.setFont(10.0f);g.drawText("BAND 0"+juce::String(band+1),4,9,getWidth()-8,15,juce::Justification::centred);
    g.setColour(muted);g.setFont(9.0f);g.drawText("FREQ",4,27,getWidth()-8,12,juce::Justification::centred);g.drawText("GAIN",4,110,getWidth()/2-6,12,juce::Justification::centred);g.drawText("Q",getWidth()/2,110,getWidth()/2-6,12,juce::Justification::centred);g.drawText("FILTER",4,181,getWidth()-8,12,juce::Justification::centred);g.drawText("SLOPE",4,209,getWidth()-8,12,juce::Justification::centred);
}
void RBTEQAudioProcessorEditor::BandStrip::resized(){
    const int w=getWidth(),h=getHeight();const int pad=4;
    freq.setBounds(pad,27,w-2*pad,78);
    gain.setBounds(pad,111,w/2-5,78);q.setBounds(w/2,111,w/2-pad,78);
    type.setBounds(pad,194,w-2*pad,24);
    slope.setBounds(pad,222,w-2*pad,22);
    on.setBounds(pad,250,40,20);solo.setBounds(w-50,250,46,20);
}

