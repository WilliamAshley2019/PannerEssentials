#include "PluginEditor.h"

PanningEditor::PanningEditor(PanningProcessor& p) :
    AudioProcessorEditor(&p), processor(p) {

    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 24);
    panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    addAndMakeVisible(panSlider);

    lawCombo.addItem("Linear", 1);
    lawCombo.addItem("Constant Power", 2);
    lawCombo.setSelectedId(2);
    addAndMakeVisible(lawCombo);

    syncButton.setButtonText("Host Sync");
    addAndMakeVisible(syncButton);

    curveGenCombo.addItem("Manual Edit", 1);
    curveGenCombo.addItem("Sine Wave", 2);
    curveGenCombo.addItem("Ramp", 3);
    curveGenCombo.addItem("Random", 4);
    curveGenCombo.addItem("Bounce", 5);
    curveGenCombo.setSelectedId(1);
    curveGenCombo.addListener(this);
    addAndMakeVisible(curveGenCombo);

    loadButton.setButtonText("Load");
    loadButton.addListener(this);
    addAndMakeVisible(loadButton);

    saveButton.setButtonText("Save");
    saveButton.addListener(this);
    addAndMakeVisible(saveButton);

    applyButton.setButtonText("Apply");
    applyButton.addListener(this);
    addAndMakeVisible(applyButton);

    generateButton.setButtonText("Generate");
    generateButton.addListener(this);
    addAndMakeVisible(generateButton);

    breakpointEditor.setMultiLine(true);
    breakpointEditor.setReturnKeyStartsNewLine(true);
    breakpointEditor.setReadOnly(false);
    breakpointEditor.setScrollbarsShown(true);
    breakpointEditor.setCaretVisible(true);
    breakpointEditor.setPopupMenuEnabled(true);
    breakpointEditor.setText(processor.getBreakpointText());
    breakpointEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    addAndMakeVisible(breakpointEditor);

    editorLabel.setText("Breakpoint Editor:", juce::dontSendNotification);
    addAndMakeVisible(editorLabel);

    infoLabel.setText("Drag .txt files here or use editor", juce::dontSendNotification);
    infoLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(infoLabel);

    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    panAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.params, "pan", panSlider);
    lawAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.params, "law", lawCombo);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.params, "sync", syncButton);
    curveGenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.params, "curvemode", curveGenCombo);

    setSize(600, 700);
    startTimerHz(30);
}

PanningEditor::~PanningEditor() {
    stopTimer();
}

void PanningEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1e1e1e));

    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("Uber Panner Pro", getLocalBounds().removeFromTop(40), juce::Justification::centred);

    graphBounds = getLocalBounds().withTrimmedTop(40).withHeight(200).reduced(10, 0);
    drawGraphBackground(g, graphBounds);
    drawWaveform(g, graphBounds);
    drawBreakpointMarkers(g, graphBounds);
    drawPanPosition(g, graphBounds, currentPanPosition);

    g.setColour(juce::Colours::grey);
    g.setFont(12.0f);
    g.drawText("L", graphBounds.getX() - 15, graphBounds.getCentreY() - 10, 10, 20, juce::Justification::centred);
    g.drawText("R", graphBounds.getRight() + 5, graphBounds.getCentreY() - 10, 10, 20, juce::Justification::centred);
    g.drawText("C", graphBounds.getCentreX() - 10, graphBounds.getCentreY() - 10, 20, 20, juce::Justification::centred);
    g.drawText("Time (s)", graphBounds.getCentreX() - 30, graphBounds.getBottom() + 5, 60, 20, juce::Justification::centred);
}

void PanningEditor::drawGraphBackground(juce::Graphics& g, const juce::Rectangle<int>& area) {
    g.setColour(juce::Colour(0xff2d2d2d));
    g.fillRect(area);

    g.setColour(juce::Colour(0xff444444));
    g.drawRect(area, 1);

    g.setColour(juce::Colour(0xff333333));

    for (int i = 0; i <= 4; ++i) {
        float y = static_cast<float>(area.getY()) + (static_cast<float>(area.getHeight()) * i / 4.0f);
        g.drawHorizontalLine(static_cast<int>(y), static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
    }

    for (int i = 0; i <= 10; ++i) {
        float x = static_cast<float>(area.getX()) + (static_cast<float>(area.getWidth()) * i / 10.0f);
        g.drawVerticalLine(static_cast<int>(x), static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));
    }

    g.setColour(juce::Colour(0xff666666));
    g.drawHorizontalLine(area.getCentreY(), static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
}

void PanningEditor::drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& area) {
    if (breakpointPath.empty()) return;

    g.setColour(juce::Colours::cyan.withAlpha(0.8f));
    juce::Path path;
    bool first = true;

    float maxTime = 0.0f;
    for (const auto& point : breakpointPath) {
        maxTime = juce::jmax(maxTime, point.first);
    }
    if (maxTime <= 0.0f) maxTime = 1.0f;

    for (const auto& point : breakpointPath) {
        float x = static_cast<float>(area.getX()) + (point.first / maxTime) * static_cast<float>(area.getWidth());
        float y = static_cast<float>(area.getY()) + static_cast<float>(area.getHeight()) * 0.5f * (1.0f - point.second);

        if (first) {
            path.startNewSubPath(x, y);
            first = false;
        }
        else {
            path.lineTo(x, y);
        }
    }

    g.strokePath(path, juce::PathStrokeType(2.0f));
}

void PanningEditor::drawBreakpointMarkers(juce::Graphics& g, const juce::Rectangle<int>& area) {
    if (breakpointPath.empty()) return;

    float maxTime = 0.0f;
    for (const auto& point : breakpointPath) {
        maxTime = juce::jmax(maxTime, point.first);
    }
    if (maxTime <= 0.0f) maxTime = 1.0f;

    for (size_t i = 0; i < breakpointPath.size(); ++i) {
        const auto& point = breakpointPath[i];
        float x = static_cast<float>(area.getX()) + (point.first / maxTime) * static_cast<float>(area.getWidth());
        float y = static_cast<float>(area.getY()) + static_cast<float>(area.getHeight()) * 0.5f * (1.0f - point.second);

        if (i == draggedBreakpoint.index && isDragging) {
            g.setColour(juce::Colours::red);
        }
        else {
            g.setColour(juce::Colours::yellow);
        }

        g.fillEllipse(x - 5, y - 5, 10, 10);
        g.setColour(juce::Colours::black);
        g.drawEllipse(x - 5, y - 5, 10, 10, 1);

        // Draw index number
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        g.drawText(juce::String(i),
            static_cast<int>(x - 10), static_cast<int>(y - 20),
            20, 15, juce::Justification::centred);
    }
}

void PanningEditor::drawPanPosition(juce::Graphics& g, const juce::Rectangle<int>& area, float pan) {
    float maxTime = 0.0f;
    for (const auto& point : breakpointPath) {
        maxTime = juce::jmax(maxTime, point.first);
    }
    if (maxTime <= 0.0f) maxTime = 1.0f;

    if (processor.params.getRawParameterValue("sync")->load() > 0.5f) {
        float currentTime = static_cast<float>(processor.getCurrentTime());
        g.setColour(juce::Colours::red.withAlpha(0.7f));
        float x = static_cast<float>(area.getX()) + (currentTime / maxTime) * static_cast<float>(area.getWidth());
        g.drawLine(x, static_cast<float>(area.getY()), x, static_cast<float>(area.getBottom()), 2.0f);
    }
    else {
        g.setColour(juce::Colours::yellow);
        float x = static_cast<float>(area.getX()) + static_cast<float>(area.getWidth()) * 0.5f * (pan + 1.0f);
        g.drawLine(x, static_cast<float>(area.getY()), x, static_cast<float>(area.getBottom()), 2.0f);

        auto gains = pan < 0 ? processor.linearPan(pan) : processor.constantPowerPan(pan);
        float leftHeight = static_cast<float>(area.getHeight()) * gains.left;
        float rightHeight = static_cast<float>(area.getHeight()) * gains.right;

        g.setColour(juce::Colours::green.withAlpha(0.3f));
        g.fillRect(static_cast<float>(area.getX()), static_cast<float>(area.getBottom()) - leftHeight,
            static_cast<float>(area.getWidth()) * 0.5f, leftHeight);

        g.setColour(juce::Colours::blue.withAlpha(0.3f));
        g.fillRect(static_cast<float>(area.getCentreX()), static_cast<float>(area.getBottom()) - rightHeight,
            static_cast<float>(area.getWidth()) * 0.5f, rightHeight);
    }
}

void PanningEditor::resized() {
    auto area = getLocalBounds();
    auto header = area.removeFromTop(40);

    graphBounds = area.removeFromTop(200).reduced(10, 10);

    auto controlRow1 = area.removeFromTop(40).reduced(10, 5);
    panSlider.setBounds(controlRow1.removeFromLeft(250));
    controlRow1.removeFromLeft(10);
    lawCombo.setBounds(controlRow1.removeFromLeft(120));
    controlRow1.removeFromLeft(10);
    syncButton.setBounds(controlRow1.removeFromLeft(100));

    auto controlRow2 = area.removeFromTop(40).reduced(10, 5);
    curveGenCombo.setBounds(controlRow2.removeFromLeft(120));
    controlRow2.removeFromLeft(10);
    generateButton.setBounds(controlRow2.removeFromLeft(80));
    controlRow2.removeFromLeft(10);
    loadButton.setBounds(controlRow2.removeFromLeft(60));
    controlRow2.removeFromLeft(5);
    saveButton.setBounds(controlRow2.removeFromLeft(60));
    controlRow2.removeFromLeft(5);
    applyButton.setBounds(controlRow2.removeFromLeft(60));

    auto statusRow = area.removeFromTop(30).reduced(10, 5);
    infoLabel.setBounds(statusRow.removeFromLeft(250));
    statusLabel.setBounds(statusRow);

    auto editorLabelRow = area.removeFromTop(25).reduced(10, 0);
    editorLabel.setBounds(editorLabelRow);

    breakpointEditor.setBounds(area.reduced(10, 10));
}

void PanningEditor::timerCallback() {
    currentPanPosition = static_cast<float>(panSlider.getValue());
    updateBreakpointDisplay();
    repaint();
}

bool PanningEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    for (const auto& file : files) {
        if (file.endsWithIgnoreCase(".txt") || file.endsWithIgnoreCase(".brk") || file.endsWithIgnoreCase(".pan")) {
            return true;
        }
    }
    return false;
}

void PanningEditor::filesDropped(const juce::StringArray& files, int, int) {
    for (const auto& file : files) {
        if (file.endsWithIgnoreCase(".txt") || file.endsWithIgnoreCase(".brk") || file.endsWithIgnoreCase(".pan")) {
            processor.loadBreakpointFile(juce::File(file));
            updateEditorText();
            updateBreakpointDisplay();
            statusLabel.setText("Loaded: " + juce::File(file).getFileName(), juce::dontSendNotification);
            break;
        }
    }
}

void PanningEditor::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) {
    if (comboBoxThatHasChanged == &curveGenCombo) {
        generateCurve();
    }
}

void PanningEditor::buttonClicked(juce::Button* button) {
    if (button == &loadButton) {
        loadBreakpointFile();
    }
    else if (button == &saveButton) {
        saveBreakpointFile();
    }
    else if (button == &applyButton) {
        applyBreakpoints();
    }
    else if (button == &generateButton) {
        generateCurve();
    }
}

int PanningEditor::findBreakpointAtPosition(juce::Point<float> position, float tolerance) {
    float maxTime = 0.0f;
    for (const auto& point : breakpointPath) {
        maxTime = juce::jmax(maxTime, point.first);
    }
    if (maxTime <= 0.0f) maxTime = 1.0f;

    for (size_t i = 0; i < breakpointPath.size(); ++i) {
        const auto& point = breakpointPath[i];
        float x = static_cast<float>(graphBounds.getX()) + (point.first / maxTime) * static_cast<float>(graphBounds.getWidth());
        float y = static_cast<float>(graphBounds.getY()) + static_cast<float>(graphBounds.getHeight()) * 0.5f * (1.0f - point.second);

        if (std::abs(x - position.x) <= tolerance && std::abs(y - position.y) <= tolerance) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

juce::Point<float> PanningEditor::timeValueToScreen(float time, float value) {
    float maxTime = 0.0f;
    for (const auto& point : breakpointPath) {
        maxTime = juce::jmax(maxTime, point.first);
    }
    if (maxTime <= 0.0f) maxTime = 1.0f;

    float x = static_cast<float>(graphBounds.getX()) + (time / maxTime) * static_cast<float>(graphBounds.getWidth());
    float y = static_cast<float>(graphBounds.getY()) + static_cast<float>(graphBounds.getHeight()) * 0.5f * (1.0f - value);

    return { x, y };
}

std::pair<float, float> PanningEditor::screenToTimeValue(juce::Point<float> screenPos) {
    float maxTime = 0.0f;
    for (const auto& point : breakpointPath) {
        maxTime = juce::jmax(maxTime, point.first);
    }
    if (maxTime <= 0.0f) maxTime = 1.0f;

    float time = ((screenPos.x - graphBounds.getX()) / graphBounds.getWidth()) * maxTime;
    float value = 1.0f - 2.0f * ((screenPos.y - graphBounds.getY()) / graphBounds.getHeight());

    time = juce::jmax(0.0f, time);
    value = juce::jlimit(-1.0f, 1.0f, value);

    return { time, value };
}

void PanningEditor::updateBreakpointFromDrag(juce::Point<float> currentPosition) {
    if (draggedBreakpoint.index >= 0 && draggedBreakpoint.index < breakpointPath.size()) {
        auto [newTime, newValue] = screenToTimeValue(currentPosition);
        processor.updateBreakpoint(draggedBreakpoint.index, newTime, newValue);
        updateEditorText();
        updateBreakpointDisplay();
        repaint();
    }
}

void PanningEditor::addBreakpointAtPosition(juce::Point<float> position) {
    if (graphBounds.contains(position.toInt())) {
        auto [time, value] = screenToTimeValue(position);
        processor.addBreakpoint(time, value);
        updateEditorText();
        updateBreakpointDisplay();
        repaint();
        statusLabel.setText("Added breakpoint at time " + juce::String(time, 2) + "s", juce::dontSendNotification);
    }
}

void PanningEditor::removeBreakpointAtPosition(juce::Point<float> position) {
    int index = findBreakpointAtPosition(position);
    if (index >= 0) {
        processor.removeBreakpoint(index);
        updateEditorText();
        updateBreakpointDisplay();
        repaint();
        statusLabel.setText("Removed breakpoint " + juce::String(index), juce::dontSendNotification);
    }
}

void PanningEditor::mouseDown(const juce::MouseEvent& event) {
    if (graphBounds.contains(event.getPosition())) {
        if (event.mods.isLeftButtonDown()) {
            int index = findBreakpointAtPosition(event.position);
            if (index >= 0) {
                draggedBreakpoint.index = index;
                draggedBreakpoint.dragStartPosition = event.position;
                draggedBreakpoint.originalTime = breakpointPath[index].first;
                draggedBreakpoint.originalValue = breakpointPath[index].second;
                isDragging = true;
                return;
            }
        }
        else if (event.mods.isRightButtonDown()) {
            removeBreakpointAtPosition(event.position);
            return;
        }
    }
}

void PanningEditor::mouseDrag(const juce::MouseEvent& event) {
    if (isDragging && event.mods.isLeftButtonDown()) {
        updateBreakpointFromDrag(event.position);
    }
}

void PanningEditor::mouseUp(const juce::MouseEvent&) {
    if (isDragging) {
        isDragging = false;
        statusLabel.setText("Breakpoint updated", juce::dontSendNotification);
    }
}

void PanningEditor::mouseDoubleClick(const juce::MouseEvent& event) {
    if (graphBounds.contains(event.getPosition()) && event.mods.isLeftButtonDown()) {
        addBreakpointAtPosition(event.position);
    }
}

void PanningEditor::loadBreakpointFile() {
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Breakpoint File",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.txt;*.brk;*.pan;*.csv"
    );

    auto folderFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(folderFlags, [this](const juce::FileChooser& chooser) {
        auto result = chooser.getResult();
        if (result.existsAsFile()) {
            processor.loadBreakpointFile(result);
            updateEditorText();
            updateBreakpointDisplay();
            statusLabel.setText("Loaded: " + result.getFileName(), juce::dontSendNotification);
        }
        });
}

void PanningEditor::saveBreakpointFile() {
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Breakpoint File",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("pan_curve.txt"),
        "*.txt;*.brk;*.pan"
    );

    auto folderFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(folderFlags, [this](const juce::FileChooser& chooser) {
        auto result = chooser.getResult();
        if (result.getFullPathName().isNotEmpty()) {
            processor.saveBreakpointFile(result);
            statusLabel.setText("Saved: " + result.getFileName(), juce::dontSendNotification);
        }
        });
}

void PanningEditor::applyBreakpoints() {
    processor.setBreakpointText(breakpointEditor.getText());
    updateBreakpointDisplay();
    statusLabel.setText("Breakpoints applied", juce::dontSendNotification);
}

void PanningEditor::generateCurve() {
    int mode = curveGenCombo.getSelectedId();

    switch (mode) {
    case 2:
        processor.generateSineCurve(5.0f, 1.0f, 0.5f);
        break;
    case 3:
        processor.generateRampCurve(5.0f, -1.0f, 1.0f);
        break;
    case 4:
        processor.generateRandomCurve(5.0f, 10.0f);
        break;
    case 5:
        processor.setBreakpointText(
            "0.0 -1.0\n"
            "0.5 0.0\n"
            "1.0 1.0\n"
            "1.5 0.0\n"
            "2.0 -1.0\n"
            "2.5 0.0\n"
            "3.0 1.0\n"
            "3.5 0.0\n"
            "4.0 -1.0\n"
            "4.5 0.0\n"
            "5.0 1.0\n"
        );
        break;
    default:
        break;
    }

    updateEditorText();
    updateBreakpointDisplay();
    statusLabel.setText("Generated curve: " + curveGenCombo.getText(), juce::dontSendNotification);
}

void PanningEditor::updateBreakpointDisplay() {
    breakpointPath.clear();
    auto points = processor.getBreakpointsForDisplay();

    for (const auto& point : points) {
        breakpointPath.push_back({ static_cast<float>(point.first), static_cast<float>(point.second) });
    }
}

void PanningEditor::updateEditorText() {
    breakpointEditor.setText(processor.getBreakpointText());
}