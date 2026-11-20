#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::String bandId(int band, const juce::String& suffix) { return "b" + juce::String(band + 1) + "_" + suffix; }

RBTEQAudioProcessor::RBTEQAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    updateFilters();
}

juce::AudioProcessorValueTreeState::ParameterLayout RBTEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const char* names[8] = { "LOW", "LOW 2", "LOW MID", "MID", "MID 2", "HIGH MID", "HIGH", "AIR" };
    const float defaults[8] = { 50, 120, 350, 900, 2200, 4800, 9000, 15000 };
    juce::ignoreUnused(names);

    for (int i = 0; i < 8; ++i)
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(bandId(i, "freq"), "Band " + juce::String(i + 1) + " Frequency",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.01f, 0.25f), defaults[i]));
        layout.add(std::make_unique<juce::AudioParameterFloat>(bandId(i, "gain"), "Band " + juce::String(i + 1) + " Gain",
            juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f), 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(bandId(i, "q"), "Band " + juce::String(i + 1) + " Q",
            juce::NormalisableRange<float>(0.1f, 10.0f, 0.001f, 0.35f), 1.0f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(bandId(i, "type"), "Band " + juce::String(i + 1) + " Type",
            juce::StringArray { "Bell", "Low Shelf", "High Shelf", "HPF", "LPF" }, 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(bandId(i, "slope"), "Band " + juce::String(i + 1) + " Slope",
            juce::StringArray { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, 3));
        layout.add(std::make_unique<juce::AudioParameterBool>(bandId(i, "on"), "Band " + juce::String(i + 1) + " On", true));
        layout.add(std::make_unique<juce::AudioParameterBool>(bandId(i, "solo"), "Band " + juce::String(i + 1) + " Solo", false));
    }

    return layout;
}

void RBTEQAudioProcessor::prepareToPlay(double sampleRate, int)
{
    sr = sampleRate;
    fifoIndex = 0;
    fifo.fill(0.0f);
    fftData.fill(0.0f);

    for (auto& band : filters)
        for (auto& stage : band)
            for (auto& channel : stage)
                channel.clear();

    for (auto& band : soloFilters)
        for (auto& channel : band)
            channel.clear();

    updateFilters();
}

void RBTEQAudioProcessor::releaseResources() {}

bool RBTEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

RBTEQAudioProcessor::Coeff RBTEQAudioProcessor::makeCoeff(int type, double frequency, double gain, double q, double sampleRate)
{
    const double f = juce::jlimit(20.0, sampleRate * 0.48, frequency);
    const double safeQ = juce::jlimit(0.1, 10.0, q);
    const double A = std::pow(10.0, gain / 40.0);
    const double w = 2.0 * juce::MathConstants<double>::pi * f / sampleRate;
    const double cw = std::cos(w);
    const double sw = std::sin(w);
    const double alpha = sw / (2.0 * safeQ);
    const double sqrtA = std::sqrt(A);

    Coeff c;

    switch (type)
    {
        case 1: // Low shelf
        {
            const double al = sw * 0.5 * std::sqrt(2.0);
            const double beta = 2.0 * sqrtA * al;
            c.b0 = A * ((A + 1.0) - (A - 1.0) * cw + beta);
            c.b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
            c.b2 = A * ((A + 1.0) - (A - 1.0) * cw - beta);
            c.a0 = (A + 1.0) + (A - 1.0) * cw + beta;
            c.a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
            c.a2 = (A + 1.0) + (A - 1.0) * cw - beta;
            break;
        }

        case 2: // High shelf
        {
            const double al = sw * 0.5 * std::sqrt(2.0);
            const double beta = 2.0 * sqrtA * al;
            c.b0 = A * ((A + 1.0) + (A - 1.0) * cw + beta);
            c.b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
            c.b2 = A * ((A + 1.0) + (A - 1.0) * cw - beta);
            c.a0 = (A + 1.0) - (A - 1.0) * cw + beta;
            c.a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
            c.a2 = (A + 1.0) - (A - 1.0) * cw - beta;
            break;
        }

        case 3: // High-pass
            c.b0 = (1.0 + cw) * 0.5;
            c.b1 = -(1.0 + cw);
            c.b2 = (1.0 + cw) * 0.5;
            c.a0 = 1.0 + alpha;
            c.a1 = -2.0 * cw;
            c.a2 = 1.0 - alpha;
            break;

        case 4: // Low-pass
            c.b0 = (1.0 - cw) * 0.5;
            c.b1 = 1.0 - cw;
            c.b2 = (1.0 - cw) * 0.5;
            c.a0 = 1.0 + alpha;
            c.a1 = -2.0 * cw;
            c.a2 = 1.0 - alpha;
            break;

        default: // Bell / peaking EQ
            c.b0 = 1.0 + alpha * A;
            c.b1 = -2.0 * cw;
            c.b2 = 1.0 - alpha * A;
            c.a0 = 1.0 + alpha / A;
            c.a1 = -2.0 * cw;
            c.a2 = 1.0 - alpha / A;
            break;
    }

    return c;
}

RBTEQAudioProcessor::Coeff RBTEQAudioProcessor::makeBandPass(double frequency, double q, double sampleRate)
{
    const double f = juce::jlimit(20.0, sampleRate * 0.48, frequency);
    const double w = 2.0 * juce::MathConstants<double>::pi * f / sampleRate;
    const double alpha = std::sin(w) / (2.0 * juce::jlimit(0.1, 10.0, q));

    Coeff c;
    c.b0 = alpha;
    c.b1 = 0.0;
    c.b2 = -alpha;
    c.a0 = 1.0 + alpha;
    c.a1 = -2.0 * std::cos(w);
    c.a2 = 1.0 - alpha;
    return c;
}

double RBTEQAudioProcessor::getMagnitudeResponseDb(double frequency) const
{
    const double f = juce::jlimit(10.0, sr * 0.49, frequency);
    const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
    const std::complex<double> z(std::cos(w), -std::sin(w));
    const auto z2 = z * z;

    const auto sectionMagnitude = [&] (const Coeff& c)
    {
        const std::complex<double> numerator = c.b0 + c.b1 * z + c.b2 * z2;
        const std::complex<double> denominator = c.a0 + c.a1 * z + c.a2 * z2;
        return std::abs(numerator / denominator);
    };

    // Solo mode replaces the EQ chain with the first active audition band-pass.
    for (int b = 0; b < 8; ++b)
    {
        if (parameters.getRawParameterValue(bandId(b, "solo"))->load() > 0.5f)
        {
            const double fc = parameters.getRawParameterValue(bandId(b, "freq"))->load();
            const double q = parameters.getRawParameterValue(bandId(b, "q"))->load();
            const double magnitude = sectionMagnitude(makeBandPass(fc, q, sr));
            return 20.0 * std::log10(std::max(magnitude, 1.0e-12));
        }
    }

    static constexpr double qFor12dB[] = { 0.7071067811865476 };
    static constexpr double qFor18dB[] = { 1.0 };
    static constexpr double qFor24dB[] = { 0.541196100146197, 1.306562964876377 };
    static constexpr double qFor36dB[] = { 0.5176380902050415, 0.7071067811865476, 1.9318516525781366 };
    static constexpr double qFor48dB[] = { 0.5097955791041592, 0.6013448869350453, 0.8999762231364156, 2.562915447741506 };

    double totalDb = 0.0;
    for (int b = 0; b < 8; ++b)
    {
        if (parameters.getRawParameterValue(bandId(b, "on"))->load() <= 0.5f)
            continue;

        const double fc = parameters.getRawParameterValue(bandId(b, "freq"))->load();
        const double gain = parameters.getRawParameterValue(bandId(b, "gain"))->load();
        const double q = parameters.getRawParameterValue(bandId(b, "q"))->load();
        const int type = static_cast<int>(parameters.getRawParameterValue(bandId(b, "type"))->load());

        if (type != 3 && type != 4)
        {
            totalDb += 20.0 * std::log10(std::max(sectionMagnitude(makeCoeff(type, fc, gain, q, sr)), 1.0e-12));
            continue;
        }

        const int slope = static_cast<int>(parameters.getRawParameterValue(bandId(b, "slope"))->load());
        const double* sectionQs = nullptr;
        int biquadCount = 0;
        bool firstOrder = false;
        switch (slope)
        {
            case 0: firstOrder = true; break;
            case 1: sectionQs = qFor12dB; biquadCount = 1; break;
            case 2: firstOrder = true; sectionQs = qFor18dB; biquadCount = 1; break;
            case 3: sectionQs = qFor24dB; biquadCount = 2; break;
            case 4: sectionQs = qFor36dB; biquadCount = 3; break;
            case 5: sectionQs = qFor48dB; biquadCount = 4; break;
            default: sectionQs = qFor24dB; biquadCount = 2; break;
        }

        double bandDb = 0.0;
        for (int stage = 0; stage < (firstOrder ? biquadCount + 1 : biquadCount); ++stage)
        {
            Coeff section;
            if (firstOrder && stage == 0)
            {
                const double k = std::tan(juce::MathConstants<double>::pi * fc / sr);
                section = {};
                if (type == 3) // 1st-order high-pass
                {
                    section.b0 = 1.0 / (1.0 + k);
                    section.b1 = -1.0 / (1.0 + k);
                }
                else // 1st-order low-pass
                {
                    section.b0 = k / (1.0 + k);
                    section.b1 = k / (1.0 + k);
                }
                section.a0 = 1.0;
                section.a1 = (1.0 - k) / (1.0 + k);
            }
            else
            {
                const int qIndex = firstOrder ? stage - 1 : stage;
                if (qIndex < biquadCount)
                    section = makeCoeff(type, fc, 0.0, sectionQs[qIndex], sr);
            }

            bandDb += 20.0 * std::log10(std::max(sectionMagnitude(section), 1.0e-12));
        }
        totalDb += bandDb;
    }

    return totalDb;
}

void RBTEQAudioProcessor::updateFilters()
{
    // Butterworth pole Q values, ordered from the lowest-Q section to the highest-Q section.
    // Slope choices map to filter orders 1, 2, 3, 4, 6 and 8 respectively.
    static constexpr double qFor12dB[] = { 0.7071067811865476 };
    static constexpr double qFor18dB[] = { 1.0 };
    static constexpr double qFor24dB[] = { 0.541196100146197, 1.306562964876377 };
    static constexpr double qFor36dB[] = { 0.5176380902050415, 0.7071067811865476, 1.9318516525781366 };
    static constexpr double qFor48dB[] = { 0.5097955791041592, 0.6013448869350453, 0.8999762231364156, 2.562915447741506 };

    for (int b = 0; b < 8; ++b)
    {
        const double f = parameters.getRawParameterValue(bandId(b, "freq"))->load();
        const double g = parameters.getRawParameterValue(bandId(b, "gain"))->load();
        const double q = parameters.getRawParameterValue(bandId(b, "q"))->load();
        const int type = static_cast<int>(parameters.getRawParameterValue(bandId(b, "type"))->load());
        const int slope = static_cast<int>(parameters.getRawParameterValue(bandId(b, "slope"))->load());

        bool hasFirstOrderSection = false;
        const double* sectionQs = nullptr;
        int biquadCount = 0;

        switch (slope)
        {
            case 0: hasFirstOrderSection = true; break;                         // 6 dB/oct, 1st order
            case 1: sectionQs = qFor12dB; biquadCount = 1; break;               // 12 dB/oct, 2nd order
            case 2: hasFirstOrderSection = true; sectionQs = qFor18dB; biquadCount = 1; break; // 18 dB/oct, 3rd order
            case 3: sectionQs = qFor24dB; biquadCount = 2; break;               // 24 dB/oct, 4th order
            case 4: sectionQs = qFor36dB; biquadCount = 3; break;               // 36 dB/oct, 6th order
            case 5: sectionQs = qFor48dB; biquadCount = 4; break;               // 48 dB/oct, 8th order
            default: sectionQs = qFor24dB; biquadCount = 2; break;
        }

        const Coeff eqCoeff = makeCoeff(type, f, g, q, sr);
        const Coeff soloCoeff = makeBandPass(f, q, sr);
        const bool isCutFilter = (type == 3 || type == 4);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& solo = soloFilters[b][ch];
            solo.b0 = soloCoeff.b0 / soloCoeff.a0;
            solo.b1 = soloCoeff.b1 / soloCoeff.a0;
            solo.b2 = soloCoeff.b2 / soloCoeff.a0;
            solo.a1 = soloCoeff.a1 / soloCoeff.a0;
            solo.a2 = soloCoeff.a2 / soloCoeff.a0;

            // Start with four identity sections. This is important: Bell and shelf
            // filters must run once, not be accidentally cascaded four times.
            for (int stage = 0; stage < 4; ++stage)
            {
                Coeff section;
                if (!isCutFilter)
                {
                    if (stage == 0)
                        section = eqCoeff;
                }
                else
                {
                    int nextStage = 0;

                    if (hasFirstOrderSection)
                    {
                        if (stage == nextStage)
                        {
                            const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
                            const double k = std::tan(w * 0.5);
                            section = {};
                            if (type == 3) // 1st-order high-pass
                            {
                                section.b0 = 1.0 / (1.0 + k);
                                section.b1 = -1.0 / (1.0 + k);
                            }
                            else // 1st-order low-pass
                            {
                                section.b0 = k / (1.0 + k);
                                section.b1 = k / (1.0 + k);
                            }
                            section.a0 = 1.0;
                            section.a1 = (1.0 - k) / (1.0 + k);
                        }
                        ++nextStage;
                    }

                    const int biquadIndex = stage - nextStage;
                    if (biquadIndex >= 0 && biquadIndex < biquadCount)
                        section = makeCoeff(type, f, 0.0, sectionQs[biquadIndex], sr);
                }

                auto& filter = filters[b][stage][ch];
                const double a0 = (std::abs(section.a0) < 1.0e-15 ? 1.0 : section.a0);
                filter.b0 = section.b0 / a0;
                filter.b1 = section.b1 / a0;
                filter.b2 = section.b2 / a0;
                filter.a1 = section.a1 / a0;
                filter.a2 = section.a2 / a0;
            }
        }

        activeSolo[b] = parameters.getRawParameterValue(bandId(b, "solo"))->load() > 0.5f;
    }
}

void RBTEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    updateFilters();

    // Read switches once per block rather than performing atomic parameter reads
    // inside the per-sample/per-band processing loop.
    std::array<bool, 8> enabled{};
    int soloBand = -1;
    for (int b = 0; b < 8; ++b)
    {
        enabled[b] = parameters.getRawParameterValue(bandId(b, "on"))->load() > 0.5f;
        if (soloBand < 0 && activeSolo[b])
            soloBand = b;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float mono = (left[i] + right[i]) * 0.5f;
        fifo[static_cast<size_t>(fifoIndex++)] = mono;

        if (fifoIndex >= fftSize)
        {
            std::copy(fifo.begin(), fifo.end(), fftData.begin());
            window.multiplyWithWindowingTable(fftData.data(), fftSize);
            fft.performFrequencyOnlyForwardTransform(fftData.data());

            std::array<float, 512> temp{};
            for (int k = 0; k < 512; ++k)
            {
                const float magnitude = fftData[static_cast<size_t>(k)] / (fftSize * 0.5f);
                temp[static_cast<size_t>(k)] = juce::jlimit(-100.0f, 0.0f,
                    20.0f * std::log10(magnitude + 1.0e-7f));
            }

            if (spectrumLock.tryEnter())
            {
                spectrum = temp;
                spectrumLock.exit();
            }
            fifoIndex = 0;
        }

        double xLeft = left[i];
        double xRight = right[i];

        if (soloBand >= 0)
        {
            xLeft = soloFilters[soloBand][0].process(xLeft);
            xRight = soloFilters[soloBand][1].process(xRight);
        }
        else
        {
            for (int b = 0; b < 8; ++b)
            {
                if (!enabled[b])
                    continue;

                for (int stage = 0; stage < 4; ++stage)
                {
                    xLeft = filters[b][stage][0].process(xLeft);
                    xRight = filters[b][stage][1].process(xRight);
                }
            }
        }

        left[i] = static_cast<float>(xLeft);
        right[i] = static_cast<float>(xRight);
    }
}

void RBTEQAudioProcessor::getSpectrum(std::array<float, 512>& out) const
{
    if (spectrumLock.tryEnter())
    {
        out = spectrum;
        spectrumLock.exit();
    }
}

float RBTEQAudioProcessor::getBandFrequency(int b) const { return parameters.getRawParameterValue(bandId(b, "freq"))->load(); }
float RBTEQAudioProcessor::getBandGain(int b) const { return parameters.getRawParameterValue(bandId(b, "gain"))->load(); }
float RBTEQAudioProcessor::getBandQ(int b) const { return parameters.getRawParameterValue(bandId(b, "q"))->load(); }
int RBTEQAudioProcessor::getBandType(int b) const { return static_cast<int>(parameters.getRawParameterValue(bandId(b, "type"))->load()); }
bool RBTEQAudioProcessor::getBandEnabled(int b) const { return parameters.getRawParameterValue(bandId(b, "on"))->load() > 0.5f; }
bool RBTEQAudioProcessor::getBandSolo(int b) const { return parameters.getRawParameterValue(bandId(b, "solo"))->load() > 0.5f; }

void RBTEQAudioProcessor::setBandFrequency(int b, float value)
{
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(bandId(b, "freq")));
    if (parameter != nullptr)
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void RBTEQAudioProcessor::setBandGain(int b, float value)
{
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(bandId(b, "gain")));
    if (parameter != nullptr)
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void RBTEQAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void RBTEQAudioProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* RBTEQAudioProcessor::createEditor()
{
    return new RBTEQAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RBTEQAudioProcessor();
}
