#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace BabyGirl
{
    /**
     * BabyGirl Custom LookAndFeel
     * Analog machined hardware knobs, knurled bezels, warm incandescent backlighting.
     */
    class BabyGirlLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        BabyGirlLookAndFeel()
        {
            setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xfff59e0b));
            setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff22242a));
            setColour(juce::Slider::thumbColourId, juce::Colour(0xffffffff));
            setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffc7c9d1));
            setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour(juce::Label::textColourId, juce::Colour(0xffc7c9d1));
            setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff15161b));
            setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe4e4e7));
            setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xfff59e0b));
            setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xff111111));
            setColour(juce::PopupMenu::headerTextColourId, juce::Colour(0xff8b8d98));
            setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1c1d24));
            setColour(juce::ComboBox::textColourId, juce::Colour(0xffe4e4e7));
            setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2f313a));
            setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0b0c10));
            setColour(juce::TextEditor::textColourId, juce::Colour(0xffe4e4e7));
            setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2f313a));
            setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xfff59e0b));
            setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0b0c10));
            setColour(juce::ListBox::outlineColourId, juce::Colour(0xff2f313a));
            setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff3f3f46));
        }

        void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced(0.5f);
            auto c = bg;
            if (b.getToggleState()) c = juce::Colour(0xfff59e0b).withAlpha(0.9f);
            if (over) c = c.brighter(0.12f);
            if (down) c = c.darker(0.2f);
            g.setGradientFill(juce::ColourGradient::vertical(c.brighter(0.08f), r.getY(), c.darker(0.12f), r.getBottom()));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(b.getToggleState() ? juce::Colour(0xfffbbf24) : juce::Colour(0xff3a3c46));
            g.drawRoundedRectangle(r, 4.0f, 1.0f);
        }

        void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
        {
            g.setFont(juce::Font(juce::jmin(12.0f, (float)b.getHeight() * 0.5f), juce::Font::bold));
            g.setColour(b.getToggleState() ? juce::Colour(0xff161616) : b.findColour(juce::TextButton::textColourOffId));
            g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(3, 0), juce::Justification::centred, 1);
        }

        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
        {
            auto r = b.getLocalBounds().toFloat();
            const float d = juce::jmin(14.0f, r.getHeight() - 2.0f);
            juce::Rectangle<float> box(r.getX() + 2.0f, r.getCentreY() - d * 0.5f, d, d);
            g.setColour(juce::Colour(0xff0b0c10)); g.fillRoundedRectangle(box, 3.0f);
            g.setColour(over ? juce::Colour(0xfffbbf24) : juce::Colour(0xff3a3c46)); g.drawRoundedRectangle(box, 3.0f, 1.0f);
            if (b.getToggleState())
            {
                g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.35f)); g.fillRoundedRectangle(box.expanded(2.0f), 4.0f);
                g.setColour(juce::Colour(0xfff59e0b)); g.fillRoundedRectangle(box.reduced(3.0f), 2.0f);
            }
            g.setColour(juce::Colour(0xffc7c9d1)); g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText(b.getButtonText(), (int)(box.getRight() + 5.0f), 0, b.getWidth() - (int)(box.getRight() + 5.0f), b.getHeight(), juce::Justification::centredLeft);
        }

        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle,
                              float rotaryEndAngle, juce::Slider& slider) override
        {
            auto radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
            auto centreX = (float)x + (float)width * 0.5f;
            auto centreY = (float)y + (float)height * 0.5f;
            auto rx = centreX - radius;
            auto ry = centreY - radius;
            auto rw = radius * 2.0f;
            auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

            // Knurled metal outer circle
            g.setColour(juce::Colour(0xff18191e));
            g.fillEllipse(rx - 2.0f, ry - 2.0f, rw + 4.0f, rw + 4.0f);

            // Dial track
            juce::Path trackPath;
            trackPath.addCentredArc(centreX, centreY, radius - 2.0f, radius - 2.0f,
                                    0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(juce::Colour(0xff262830));
            g.strokePath(trackPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Active glow arc
            juce::Path activePath;
            activePath.addCentredArc(centreX, centreY, radius - 2.0f, radius - 2.0f,
                                     0.0f, rotaryStartAngle, angle, true);
            g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
            g.strokePath(activePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Machined aluminum center body
            juce::ColourGradient grad(juce::Colour(0xff3a3c45), centreX - radius * 0.5f, centreY - radius * 0.5f,
                                      juce::Colour(0xff14151a), centreX + radius * 0.5f, centreY + radius * 0.5f, true);
            g.setGradientFill(grad);
            g.fillEllipse(rx + 4.0f, ry + 4.0f, rw - 8.0f, rw - 8.0f);

            // Pointer line
            juce::Path p;
            auto pointerLength = radius * 0.75f;
            auto pointerThickness = 2.5f;
            p.addRectangle(-pointerThickness * 0.5f, -radius + 4.0f, pointerThickness, pointerLength * 0.6f);
            p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));

            g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
            g.fillPath(p);

            // Center screw recess
            g.setColour(juce::Colour(0xff090a0d));
            g.fillEllipse(centreX - 3.0f, centreY - 3.0f, 6.0f, 6.0f);
        }
    };
}
