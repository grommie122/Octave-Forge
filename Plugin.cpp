#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

namespace
{
struct Control
{
    const char* id;
    const char* name;
    float minimum, maximum, initial;
};

constexpr std::array<Control, 4> controls {{
    {"tuning", "Down tune (st)", -12, 0, 0},
    {"fundamental", "Fundamental (%)", 0, 100, 100},
    {"up", "Octave up (%)", 0, 100, 0},
    {"down", "Octave down (%)", 0, 100, 0}
}};

using State = juce::AudioProcessorValueTreeState;

State::ParameterLayout makeParameters()
{
    State::ParameterLayout layout;
    for (const auto& control : controls)
    {
        const juce::ParameterID id {control.id, 1};
        layout.add(std::make_unique<juce::AudioParameterFloat>(id, control.name,
            juce::NormalisableRange<float>{control.minimum, control.maximum, control.minimum < 0 ? 1.0f : 0.1f},
            control.initial));
    }
    return layout;
}

class OctaveEditor final : public juce::AudioProcessorEditor
{
public:
    OctaveEditor(juce::AudioProcessor& processor, State& state) : AudioProcessorEditor(processor)
    {
        for (size_t i = 0; i < knobs.size(); ++i)
        {
            const auto& control = controls[knobIndices[i]];
            auto& knob = knobs[i];
            knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 23);
            knob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffc5a9ff));
            knob.setName(control.name);
            addAndMakeVisible(knob);
            knobLinks[i] = std::make_unique<State::SliderAttachment>(state, control.id, knob);
        }
        setSize(580, 264);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff151225));
        g.setColour(juce::Colours::white);
        g.setFont(26.0f);
        g.drawText("Octave Forge", 20, 12, 580, 38, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff2a2142));
        g.fillRoundedRectangle(20, 60, 540, 184, 12);
        g.setColour(juce::Colours::white);
        g.setFont(17.0f);
        g.drawText("Pitch and octaves", 34, 67, 120, 25, juce::Justification::centredLeft);
        g.setFont(13.0f);
        for (size_t i = 0; i < knobs.size(); ++i)
            g.drawText(controls[knobIndices[i]].name, knobX[i], 208, 106, 23, juce::Justification::centred);
    }

    void resized() override
    {
        for (size_t i = 0; i < knobs.size(); ++i)
            knobs[i].setBounds(knobX[i], 102, 106, 103);
    }

private:
    static constexpr std::array<size_t, 4> knobIndices {0, 1, 2, 3};
    static constexpr std::array<int, 4> knobX {36, 169, 302, 435};
    std::array<juce::Slider, 4> knobs;
    std::array<std::unique_ptr<State::SliderAttachment>, 4> knobLinks;
};

class OctaveForge final : public juce::AudioProcessor
{
public:
    OctaveForge() : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
        state(*this, nullptr, "OctaveForge", makeParameters())
    {
        for (size_t i = 0; i < controls.size(); ++i)
            values[i] = state.getRawParameterValue(controls[i].id);
    }

    const juce::String getName() const override { return "Octave Forge"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.08; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return new OctaveEditor(*this, state); }
    void releaseResources() override {}

    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    {
        const auto output = layout.getMainOutputChannelSet();
        return layout.getMainInputChannelSet() == output
            && (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo());
    }

    void prepareToPlay(double sampleRate, int) override
    {
        grain = juce::jmax(32, static_cast<int>(0.055 * sampleRate));
        history.setSize(2, grain + 8);
        history.clear();
        write = 0;
        phase.fill(0.0f);
        for (size_t voice = 0; voice < gain.size(); ++voice)
        {
            gain[voice].reset(sampleRate, 0.02);
            gain[voice].setCurrentAndTargetValue(values[voice + 1]->load() * 0.01f);
        }
        tuning.reset(sampleRate, 0.035);
        tuning.setCurrentAndTargetValue(values[0]->load());
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        const auto length = history.getNumSamples();
        tuning.setTargetValue(values[0]->load());
        for (size_t voice = 0; voice < gain.size(); ++voice)
            gain[voice].setTargetValue(values[voice + 1]->load() * 0.01f);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto ratio = std::exp2(tuning.getNextValue() / 12.0f);
            const std::array<float, 3> ratios {ratio, ratio * 2.0f, ratio * 0.5f};
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                history.setSample(channel, write, buffer.getSample(channel, sample));

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample(channel, sample, 0.0f);

            for (size_t voice = 0; voice < ratios.size(); ++voice)
            {
                const auto level = gain[voice].getNextValue();
                const auto first = phase[voice];
                const auto second = first < 0.5f ? first + 0.5f : first - 0.5f;
                const auto weight = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * first);
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                {
                    const auto shifted = ratios[voice] == 1.0f
                        ? history.getSample(channel, write)
                        : weight * read(channel, first) + (1.0f - weight) * read(channel, second);
                    buffer.addSample(channel, sample, level * shifted);
                }
                phase[voice] += (1.0f - ratios[voice]) / static_cast<float>(grain);
                phase[voice] -= std::floor(phase[voice]);
            }
            write = (write + 1) % length;
        }
    }
    void getStateInformation(juce::MemoryBlock& destination) override
    {
        if (auto xml = state.copyState().createXml())
            copyXmlToBinary(*xml, destination);
    }

    void setStateInformation(const void* data, int bytes) override
    {
        if (auto xml = getXmlFromBinary(data, bytes))
            if (xml->hasTagName(state.state.getType()))
                state.replaceState(juce::ValueTree::fromXml(*xml));
    }

private:
    State state;
    std::array<std::atomic<float>*, 4> values {};
    float read(int channel, float position) const
    {
        const auto length = history.getNumSamples();
        auto index = static_cast<float>(write) - 2.0f - position * static_cast<float>(grain);
        if (index < 0.0f)
            index += static_cast<float>(length);
        const auto left = static_cast<int>(index);
        const auto fraction = index - static_cast<float>(left);
        const auto a = history.getSample(channel, left);
        const auto b = history.getSample(channel, (left + 1) % length);
        return a + fraction * (b - a);
    }

    juce::AudioBuffer<float> history;
    std::array<juce::SmoothedValue<float>, 3> gain;
    juce::SmoothedValue<float> tuning;
    std::array<float, 3> phase {};
    int grain = 0;
    int write = 0;
};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OctaveForge();
}




