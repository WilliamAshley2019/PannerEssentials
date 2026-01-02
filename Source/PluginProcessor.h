#pragma once
#include <JuceHeader.h>

class PanningProcessor : public juce::AudioProcessor {
public:
    enum Parameters { idxPan, idxLaw, idxSync, idxCurveMode };

    PanningProcessor();
    ~PanningProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UberPanner"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    bool supportsDoublePrecisionProcessing() const override { return false; }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorValueTreeState params;

    void loadBreakpointFile(const juce::File& file);
    void saveBreakpointFile(const juce::File& file);
    juce::String getBreakpointText() const;
    void setBreakpointText(const juce::String& text);
    void generateSineCurve(float duration = 5.0f, float amplitude = 1.0f, float frequency = 0.5f);
    void generateRampCurve(float duration = 5.0f, float start = -1.0f, float end = 1.0f);
    void generateRandomCurve(float duration = 5.0f, float density = 10.0f);

    // Interactive editing
    std::vector<std::pair<double, double>> getBreakpointsForDisplay() const;
    void updateBreakpoint(size_t index, double time, double value);
    void addBreakpoint(double time, double value);
    void removeBreakpoint(size_t index);
    void sortBreakpoints();

    // Public helper functions for editor
    struct PanGains { float left; float right; };
    PanGains linearPan(float position) const;
    PanGains constantPowerPan(float position) const;

    // Public access to current time for editor visualization
    double getCurrentTime() const { return currentTime.load(); }

private:
    struct Breakpoint { double time; double value; };
    std::vector<Breakpoint> breakpoints;
    bool breakpointsLoaded = false;
    std::atomic<double> currentTime{ 0.0 };
    double timeIncrement = 0.0;
    size_t currentBreakpointIndex = 0;

    float getBreakpointValue(double time);
    void parseBreakpointText(const juce::String& text);

    juce::LinearSmoothedValue<float> smoothedPan;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanningProcessor)
};