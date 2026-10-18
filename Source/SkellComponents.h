#pragma once
#include <JuceHeader.h>

class SkellMeter : public juce::Component
{
public:
    void setLevel (float newLevel)
    {
        // Peak (linear) -> dB scale (-48..0 dB) with instant attack and smooth fall
        const float db = juce::Decibels::gainToDecibels (newLevel, -48.0f);
        const float target = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        level = (target > level) ? target : level * 0.88f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0a120a));
        g.fillRoundedRectangle (bounds, 2.0f);

        const int numSegments = 18;
        const float gap = 1.5f;
        const float segHeight = (bounds.getHeight() - (gap * (numSegments + 1))) / numSegments;
        const int litSegments = static_cast<int>(level * numSegments);

        for (int i = 0; i < numSegments; ++i)
        {
            float yPos = bounds.getBottom() - ((i + 1) * (segHeight + gap));
            bool isLit = i < litSegments;
            juce::Colour ledColour = (i >= 16) ? juce::Colour (0xffff3333) : juce::Colour (0xff39ff14);
            g.setColour (isLit ? ledColour : ledColour.withAlpha (0.15f));
            g.fillRect (bounds.getX() + 1.5f, yPos, bounds.getWidth() - 3.0f, segHeight);
        }
    }

private:
    float level = 0.0f;
};

// --- Compact echo visualiser ---------------------------------------------------
// Bars = the repeat train, decaying with FEEDBACK (ducking trims repeats after the first).
// A pulse sweeps across the taps at the
// real delay time so you can see the tempo of the echoes.
//   Normal / Inverted : mirrored bars about the centre line
//   Ping Pong         : repeats alternate up (L) / down (R)
class SkellDisplay : public juce::Component, private juce::Timer
{
public:
    SkellDisplay() { startTimerHz (30); }
    ~SkellDisplay() override { stopTimer(); }

    void setParams (float timeMs, float feedback, float ducking, int newFbMode)
    {
        timeValueMs = juce::jmax (10.0f, timeMs);
        feedbackVal = feedback;
        duckingVal = ducking;
        fbMode = newFbMode;
    }

    void paint (juce::Graphics& g) override
    {
        const auto lime = juce::Colour (0xff39ff14);
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);

        g.setColour (juce::Colour (0xff040a03));
        g.fillRoundedRectangle (bounds, 5.0f);

        auto area = bounds.reduced (7.0f, 5.0f);
        const float midY = area.getCentreY();
        const float halfH = area.getHeight() * 0.5f;

        // faint grid: centre line + vertical tick marks
        g.setColour (lime.withAlpha (0.12f));
        g.drawHorizontalLine (juce::roundToInt (midY), area.getX(), area.getRight());

        const int numTaps = 12;
        const float barW = 5.0f;
        const float step = (area.getWidth() - barW) / (numTaps - 1);
        const float fb = juce::jlimit (0.05f, 1.0f, feedbackVal);
        const float duckFactor = 1.0f - duckingVal * 0.4f;

        // sweeping pulse, one tap per delay time
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        const double phase = std::fmod (nowMs / timeValueMs, static_cast<double>(numTaps));
        const int activeTap = static_cast<int>(phase);
        const float pulse = 1.0f - static_cast<float>(phase - activeTap);   // 1 -> 0 after each hit

        for (int i = 0; i < numTaps; ++i)
        {
            const float level = std::pow (fb, static_cast<float>(i)) * (i > 0 ? duckFactor : 1.0f);
            const float h = juce::jmax (1.5f, halfH * level);
            const float x = area.getX() + i * step;

            float top, bottom;
            if (fbMode == 2)
            {
                const bool up = (i % 2 == 0);
                top    = up ? midY - h : midY;
                bottom = up ? midY     : midY + h;
            }
            else
            {
                top = midY - h;  bottom = midY + h;
            }

            const float boost = (i == activeTap) ? pulse : 0.0f;
            g.setColour (lime.withAlpha (juce::jlimit (0.0f, 1.0f, 0.22f + 0.55f * level + 0.35f * boost)));
            g.fillRoundedRectangle (x, top, barW, bottom - top, 1.0f);

            if (boost > 0.05f)   // hot cap + glow on the active tap
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f * boost));
                g.fillRect (x, (fbMode == 2 && i % 2 == 1) ? bottom - 1.5f : top, barW, 1.5f);
                g.setColour (lime.withAlpha (0.18f * boost));
                g.fillRoundedRectangle (x - 2.0f, top - 2.0f, barW + 4.0f, bottom - top + 4.0f, 2.0f);
            }
        }

        // single hairline frame
        g.setColour (lime.withAlpha (0.25f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
    }

private:
    void timerCallback() override { repaint(); }

    float timeValueMs = 250.0f;
    float feedbackVal = 0.5f;
    float duckingVal = 0.0f;
    int fbMode = 0;
};
