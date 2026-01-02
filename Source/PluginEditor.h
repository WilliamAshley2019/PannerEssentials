#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PanningEditor : public juce::AudioProcessorEditor,
    private juce::Timer,
    private juce::FileDragAndDropTarget,
    private juce::ComboBox::Listener,
    private juce::Button::Listener {
public:
    explicit PanningEditor(PanningProcessor&);
    ~PanningEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Mouse interaction
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

private:
    PanningProcessor& processor;

    juce::Slider panSlider;
    juce::ComboBox lawCombo;
    juce::ToggleButton syncButton;
    juce::ComboBox curveGenCombo;

    juce::TextButton loadButton;
    juce::TextButton saveButton;
    juce::TextButton applyButton;
    juce::TextButton generateButton;

    juce::TextEditor breakpointEditor;
    juce::Label editorLabel;

    juce::Label infoLabel;
    juce::Label statusLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lawAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> curveGenAttachment;

    std::unique_ptr<juce::FileChooser> fileChooser;

    // Visualization and interaction
    std::vector<std::pair<float, float>> breakpointPath;
    float currentPanPosition = 0.0f;
    juce::Rectangle<int> graphBounds;

    // Interactive editing state
    struct DraggedBreakpoint {
        int index = -1;
        juce::Point<float> dragStartPosition;
        float originalTime = 0.0f;
        float originalValue = 0.0f;
    };
    DraggedBreakpoint draggedBreakpoint;
    bool isDragging = false;

    void timerCallback() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int, int) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void buttonClicked(juce::Button* button) override;

    // Interactive editing methods
    int findBreakpointAtPosition(juce::Point<float> position, float tolerance = 8.0f);
    juce::Point<float> timeValueToScreen(float time, float value);
    std::pair<float, float> screenToTimeValue(juce::Point<float> screenPos);
    void updateBreakpointFromDrag(juce::Point<float> currentPosition);
    void addBreakpointAtPosition(juce::Point<float> position);
    void removeBreakpointAtPosition(juce::Point<float> position);

    void loadBreakpointFile();
    void saveBreakpointFile();
    void applyBreakpoints();
    void generateCurve();
    void updateBreakpointDisplay();
    void updateEditorText();

    void drawGraphBackground(juce::Graphics& g, const juce::Rectangle<int>& area);
    void drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& area);
    void drawPanPosition(juce::Graphics& g, const juce::Rectangle<int>& area, float pan);
    void drawBreakpointMarkers(juce::Graphics& g, const juce::Rectangle<int>& area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanningEditor)
};