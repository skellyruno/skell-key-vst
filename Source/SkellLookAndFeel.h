#pragma once
#include <JuceHeader.h>

class SkellLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SkellLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff39ff14));
        setColour (juce::Slider::thumbColourId, juce::Colour (0xff39ff14));
        setColour (juce::Label::textColourId, juce::Colour (0xff39ff14));
        setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff39ff14));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        const auto radius = juce::jmin (width, height) / 2.0f - 4.0f;
        const auto centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const auto centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;

        const bool isInteracting = slider.isMouseOverOrDragging();
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // 1. Dark Background Track
        juce::Path bgTrack;
        bgTrack.addCentredArc (centreX, centreY, radius - 2.0f, radius - 2.0f, 0.0f,
                               rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xff081404));
        g.strokePath (bgTrack, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 2. Active Lime Green Arc
        const bool isBipolar = (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0);
        const float zeroAngle = isBipolar ? (rotaryStartAngle + rotaryEndAngle) * 0.5f : rotaryStartAngle;

        if (std::abs (angle - zeroAngle) > 0.001f)
        {
            juce::Path activeTrack;
            activeTrack.addCentredArc (centreX, centreY, radius - 2.0f, radius - 2.0f, 0.0f,
                                       juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);

            juce::Colour limeColor = isInteracting ? juce::Colour (0xff66ff33) : juce::Colour (0xff39ff14);
            g.setColour (limeColor);
            g.strokePath (activeTrack, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            if (isInteracting)
            {
                g.setColour (limeColor.withAlpha (0.45f));
                g.strokePath (activeTrack, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        // 3. Inner Cap Body
        const auto capRadius = radius - 6.0f;
        const auto capX = centreX - capRadius;
        const auto capY = centreY - capRadius;
        const auto capW = capRadius * 2.0f;

        g.setColour (isInteracting ? juce::Colour (0xff102408) : juce::Colour (0xff081204));
        g.fillEllipse (capX, capY, capW, capW);

        // Rim Border
        g.setColour (isInteracting ? juce::Colour (0xff39ff14) : juce::Colour (0xff184a08));
        g.drawEllipse (capX, capY, capW, capW, isInteracting ? 1.5f : 1.0f);

        if (isInteracting)
        {
            g.setColour (juce::Colour (0xff39ff14).withAlpha (0.25f));
            g.drawEllipse (capX - 1.5f, capY - 1.5f, capW + 3.0f, capW + 3.0f, 1.0f);
        }

        // 4. Indicator Needle
        juce::Path p;
        p.addRectangle (-1.25f, -capRadius + 2.0f, 2.5f, capRadius * 0.38f);
        p.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));

        g.setColour (isInteracting ? juce::Colour (0xffffffff) : juce::Colour (0xff39ff14));
        g.fillPath (p);

        // 5. Centered Value Readout (HOVER / TURN ONLY)
        if (isInteracting)
        {
            juce::String valText = slider.getTextFromValue (slider.getValue());
            float fontSize = juce::jlimit (9.0f, 13.0f, capRadius * 0.65f);

            g.setFont (juce::FontOptions (fontSize).withStyle ("Bold"));
            g.setColour (juce::Colour (0xffffffff));

            juce::Rectangle<int> textBounds (juce::roundToInt (capX),
                                             juce::roundToInt (capY),
                                             juce::roundToInt (capW),
                                             juce::roundToInt (capW));
            g.drawText (valText, textBounds, juce::Justification::centred, false);
        }
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        auto bounds = button.getLocalBounds().toFloat();
        auto boxSize = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto boxRect = juce::Rectangle<float> ((bounds.getWidth() - boxSize) * 0.5f,
                                               (bounds.getHeight() - boxSize) * 0.5f,
                                               boxSize, boxSize).reduced (1.0f);

        const bool isChecked = button.getToggleState();

        g.setColour (juce::Colour (0xff050c03));
        g.fillRoundedRectangle (boxRect, 3.0f);

        g.setColour (isChecked ? juce::Colour (0xff39ff14) : juce::Colour (0xff155e05));
        g.drawRoundedRectangle (boxRect, 3.0f, 1.5f);

        if (isChecked)
        {
            auto fillRect = boxRect.reduced (3.0f);
            g.setColour (juce::Colour (0xff39ff14));
            g.fillRoundedRectangle (fillRect, 2.0f);

            g.setColour (juce::Colour (0xff39ff14).withAlpha (0.35f));
            g.drawRoundedRectangle (boxRect.expanded (1.5f), 4.0f, 1.0f);
        }
    }

    // Segmented control: buttons joined via setConnectedEdges share one outline
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                const juce::Colour& backgroundColour,
                                bool shouldDrawButtonAsHighlighted,
                                bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (backgroundColour, shouldDrawButtonAsDown);

        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const bool isActive = button.getToggleState();
        const int edges = button.getConnectedEdgeFlags();
        const bool l = (edges & juce::Button::ConnectedOnLeft)  == 0;
        const bool r = (edges & juce::Button::ConnectedOnRight) == 0;

        juce::Path shape;
        shape.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                                   4.0f, 4.0f, l, r, l, r);

        if (isActive)
            g.setColour (juce::Colour (0xff39ff14));
        else
            g.setColour (shouldDrawButtonAsHighlighted ? juce::Colour (0xff12280a) : juce::Colour (0xff091404));
        g.fillPath (shape);

        g.setColour (juce::Colour (0xff39ff14).withAlpha (0.3f));
        g.strokePath (shape, juce::PathStrokeType (1.0f));
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

        const bool isActive = button.getToggleState();

        g.setColour (isActive ? juce::Colour (0xff040a02) : juce::Colour (0xff2ccb0f));
        g.setFont (juce::FontOptions (8.0f).withStyle ("Bold"));
        g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
    }
};
