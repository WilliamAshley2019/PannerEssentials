#include "PluginProcessor.h"
#include "PluginEditor.h"

// FIX #1: Add explicit bus configuration to constructor
PanningProcessor::PanningProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)   // Accept stereo input
        .withInput("Input", juce::AudioChannelSet::mono(), true)     // Accept mono input
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)) // Output is stereo
    , params(*this, nullptr, "PARAMS", {
    std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pan", 1},
        "Pan Position",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 2); })
            .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue(); })
    ),
    std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"law", 1},
        "Pan Law",
        juce::StringArray{"Linear", "Constant Power"},
        1,
        juce::AudioParameterChoiceAttributes()
    ),
    std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"sync", 1},
        "Host Sync",
        false,
        juce::AudioParameterBoolAttributes()
    ),
    std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"curvemode", 1},
        "Curve Mode",
        juce::StringArray{"Manual", "Sine", "Ramp", "Random", "Bounce"},
        0,
        juce::AudioParameterChoiceAttributes()
    )
        }) {
    juce::String defaultText =
        "# Breakpoint file format:\n"
        "# time(seconds) value(-1.0 to 1.0)\n"
        "# -1.0 = full left, 0.0 = center, 1.0 = full right\n"
        "# Lines starting with '#' are comments\n"
        "# Example: pan from left to right over 5 seconds\n"
        "0.0 -1.0\n"
        "5.0 1.0\n";
    setBreakpointText(defaultText);
}

PanningProcessor::~PanningProcessor() {}

bool PanningProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    // Only allow: mono→stereo or stereo→stereo
    return (out == juce::AudioChannelSet::stereo()) &&
        (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void PanningProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    // FIX #2: Correct LinearSmoothedValue initialization
    smoothedPan.reset(sampleRate, 0.05); // Fixed: sampleRate, rampLengthInSeconds
    smoothedPan.setCurrentAndTargetValue(0.0f); // Also set initial value
    timeIncrement = 1.0 / sampleRate;
    currentBreakpointIndex = 0;
}

void PanningProcessor::releaseResources() {}

PanningProcessor::PanGains PanningProcessor::linearPan(float position) const {
    position *= 0.5f;
    return { 0.5f - position, 0.5f + position };
}

PanningProcessor::PanGains PanningProcessor::constantPowerPan(float position) const {
    constexpr float piOverFour = juce::MathConstants<float>::pi * 0.25f;
    constexpr float sqrt2Over2 = juce::MathConstants<float>::sqrt2 * 0.5f;
    float angle = position * piOverFour;
    float sinAngle = std::sin(angle);
    float cosAngle = std::cos(angle);
    return { sqrt2Over2 * (cosAngle - sinAngle), sqrt2Over2 * (cosAngle + sinAngle) };
}

float PanningProcessor::getBreakpointValue(double time) {
    if (breakpoints.size() < 2) return 0.0f;
    while (currentBreakpointIndex + 1 < breakpoints.size() && time > breakpoints[currentBreakpointIndex + 1].time) {
        ++currentBreakpointIndex;
    }
    if (currentBreakpointIndex >= breakpoints.size() - 1) return static_cast<float>(breakpoints.back().value);
    const auto& left = breakpoints[currentBreakpointIndex];
    const auto& right = breakpoints[currentBreakpointIndex + 1];
    if (right.time - left.time == 0.0) return static_cast<float>(right.value);
    double fraction = (time - left.time) / (right.time - left.time);
    return static_cast<float>(left.value + (right.value - left.value) * fraction);
}

void PanningProcessor::parseBreakpointText(const juce::String& text) {
    breakpoints.clear();
    auto lines = juce::StringArray::fromLines(text);
    double lastTime = -1.0;

    for (const auto& line : lines) {
        juce::String trimmed = line.trim();
        if (trimmed.isEmpty() || trimmed.startsWithChar('#')) continue;

        auto tokens = juce::StringArray::fromTokens(trimmed, true);
        if (tokens.size() >= 2) {
            double time = tokens[0].getDoubleValue();
            double value = juce::jlimit(-1.0, 1.0, tokens[1].getDoubleValue());

            if (time >= lastTime) {
                breakpoints.push_back({ time, value });
                lastTime = time;
            }
        }
    }

    sortBreakpoints();
    breakpointsLoaded = !breakpoints.empty();
    currentBreakpointIndex = 0;
}

juce::String PanningProcessor::getBreakpointText() const {
    juce::String text;
    text << "# Breakpoint file for UberPanner\n";
    text << "# Format: time(seconds) value(-1.0 to 1.0)\n";
    text << "# Generated: " << juce::Time::getCurrentTime().toString(true, true) << "\n";
    text << "# Lines starting with '#' are ignored\n\n";

    for (const auto& point : breakpoints) {
        text << juce::String(point.time, 3) << " " << juce::String(point.value, 3) << "\n";
    }
    return text;
}

void PanningProcessor::setBreakpointText(const juce::String& text) {
    parseBreakpointText(text);
}

void PanningProcessor::loadBreakpointFile(const juce::File& file) {
    juce::FileInputStream stream(file);
    if (stream.openedOk()) {
        juce::String content = stream.readEntireStreamAsString();
        setBreakpointText(content);
    }
}

void PanningProcessor::saveBreakpointFile(const juce::File& file) {
    juce::FileOutputStream stream(file);
    if (stream.openedOk()) {
        stream.writeText(getBreakpointText(), false, false, "\n");
    }
}

void PanningProcessor::generateSineCurve(float duration, float amplitude, float frequency) {
    breakpoints.clear();
    const int points = 32;
    for (int i = 0; i <= points; ++i) {
        float t = duration * (float)i / (float)points;
        float value = amplitude * std::sin(juce::MathConstants<float>::twoPi * frequency * t);
        breakpoints.push_back({ t, juce::jlimit(-1.0f, 1.0f, value) });
    }
    sortBreakpoints();
    breakpointsLoaded = true;
    currentBreakpointIndex = 0;
}

void PanningProcessor::generateRampCurve(float duration, float start, float end) {
    breakpoints.clear();
    breakpoints.push_back({ 0.0, start });
    breakpoints.push_back({ duration, end });
    breakpointsLoaded = true;
    currentBreakpointIndex = 0;
}

void PanningProcessor::generateRandomCurve(float duration, float density) {
    breakpoints.clear();
    breakpoints.push_back({ 0.0, 0.0 });

    int numPoints = static_cast<int>(duration * density);
    for (int i = 1; i <= numPoints; ++i) {
        float t = duration * (float)i / (float)numPoints;
        float value = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
        breakpoints.push_back({ t, value });
    }
    sortBreakpoints();
    breakpointsLoaded = true;
    currentBreakpointIndex = 0;
}

std::vector<std::pair<double, double>> PanningProcessor::getBreakpointsForDisplay() const {
    std::vector<std::pair<double, double>> result;
    for (const auto& point : breakpoints) {
        result.push_back({ point.time, point.value });
    }
    return result;
}

void PanningProcessor::updateBreakpoint(size_t index, double time, double value) {
    if (index < breakpoints.size()) {
        breakpoints[index].time = juce::jmax(0.0, time);
        breakpoints[index].value = juce::jlimit(-1.0, 1.0, value);
        sortBreakpoints();
    }
}

void PanningProcessor::addBreakpoint(double time, double value) {
    breakpoints.push_back({ juce::jmax(0.0, time), juce::jlimit(-1.0, 1.0, value) });
    sortBreakpoints();
    breakpointsLoaded = true;
}

void PanningProcessor::removeBreakpoint(size_t index) {
    if (index < breakpoints.size()) {
        breakpoints.erase(breakpoints.begin() + index);
        breakpointsLoaded = !breakpoints.empty();
        currentBreakpointIndex = 0;
    }
}

void PanningProcessor::sortBreakpoints() {
    std::sort(breakpoints.begin(), breakpoints.end(),
        [](const Breakpoint& a, const Breakpoint& b) { return a.time < b.time; });
}

void PanningProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    // DEBUG: Uncomment to test audio path
    /*
    const int numSamples = buffer.getNumSamples();
    const int totalOutputChannels = getTotalNumOutputChannels();
    static float phase = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float sample = std::sin(phase) * 0.1f;
        phase += 0.1f;
        for (int ch = 0; ch < totalOutputChannels; ++ch) {
            buffer.setSample(ch, i, sample);
        }
    }
    return;
    */

    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int totalInputChannels = getTotalNumInputChannels();
    const int totalOutputChannels = getTotalNumOutputChannels();

    if (totalOutputChannels < 2) return;

    bool useBreakpoints = breakpointsLoaded && params.getRawParameterValue("sync")->load() > 0.5f;
    bool isConstantPower = params.getRawParameterValue("law")->load() > 0.5f;

    if (!useBreakpoints) {
        float targetPan = params.getRawParameterValue("pan")->load();
        smoothedPan.setTargetValue(targetPan);

        if (totalInputChannels == 1 && totalOutputChannels >= 2) {
            auto* input = buffer.getReadPointer(0);
            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getWritePointer(1);

            for (int i = 0; i < numSamples; ++i) {
                float currentPan = smoothedPan.getNextValue();
                auto gains = isConstantPower ? constantPowerPan(currentPan) : linearPan(currentPan);
                float sample = input[i];
                left[i] = sample * gains.left;
                right[i] = sample * gains.right;
            }

            for (int ch = 2; ch < totalOutputChannels; ++ch) {
                buffer.clear(ch, 0, numSamples);
            }
        }
        else if (totalInputChannels >= 2 && totalOutputChannels >= 2) {
            auto* leftIn = buffer.getReadPointer(0);
            auto* rightIn = buffer.getReadPointer(1);
            auto* leftOut = buffer.getWritePointer(0);
            auto* rightOut = buffer.getWritePointer(1);

            for (int i = 0; i < numSamples; ++i) {
                float currentPan = smoothedPan.getNextValue();
                auto gains = isConstantPower ? constantPowerPan(currentPan) : linearPan(currentPan);
                leftOut[i] = leftIn[i] * gains.left;
                rightOut[i] = rightIn[i] * gains.right;
            }

            for (int ch = 2; ch < totalOutputChannels; ++ch) {
                buffer.clear(ch, 0, numSamples);
            }
        }
    }
    else {
        // FIX #3: Robust playhead time calculation with validation
        double blockStartTime = 0.0;
        if (auto* playhead = getPlayHead()) {
            auto positionInfo = playhead->getPosition();
            if (positionInfo.hasValue()) {
                auto timeInSeconds = positionInfo->getTimeInSeconds();
                blockStartTime = timeInSeconds.orFallback(0.0);

                // Validate time is reasonable
                if (!std::isfinite(blockStartTime) || blockStartTime < 0.0) {
                    blockStartTime = currentTime.load(std::memory_order_relaxed);
                }
            }
        }

        currentTime.store(blockStartTime, std::memory_order_relaxed);

        double sampleTime = blockStartTime;

        if (totalInputChannels == 1 && totalOutputChannels >= 2) {
            auto* input = buffer.getReadPointer(0);
            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getWritePointer(1);

            for (int i = 0; i < numSamples; ++i) {
                float currentPan = getBreakpointValue(sampleTime);
                auto gains = isConstantPower ? constantPowerPan(currentPan) : linearPan(currentPan);
                float sample = input[i];
                left[i] = sample * gains.left;
                right[i] = sample * gains.right;
                sampleTime += timeIncrement;
            }

            for (int ch = 2; ch < totalOutputChannels; ++ch) {
                buffer.clear(ch, 0, numSamples);
            }
        }
        else if (totalInputChannels >= 2 && totalOutputChannels >= 2) {
            auto* leftIn = buffer.getReadPointer(0);
            auto* rightIn = buffer.getReadPointer(1);
            auto* leftOut = buffer.getWritePointer(0);
            auto* rightOut = buffer.getWritePointer(1);

            for (int i = 0; i < numSamples; ++i) {
                float currentPan = getBreakpointValue(sampleTime);
                auto gains = isConstantPower ? constantPowerPan(currentPan) : linearPan(currentPan);
                leftOut[i] = leftIn[i] * gains.left;
                rightOut[i] = rightIn[i] * gains.right;
                sampleTime += timeIncrement;
            }

            for (int ch = 2; ch < totalOutputChannels; ++ch) {
                buffer.clear(ch, 0, numSamples);
            }
        }
    }
}

void PanningProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = params.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PanningProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName(params.state.getType())) {
        params.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

juce::AudioProcessorEditor* PanningProcessor::createEditor() {
    return new PanningEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PanningProcessor();
}