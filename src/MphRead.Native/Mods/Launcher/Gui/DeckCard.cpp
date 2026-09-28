#include "DeckCard.hpp"

#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::MathMax;
    using ::MphRead::NativeRuntime::MathMin;

    DeckCard::DeckCard()
    {
        static const bool registered = []
        {
            AffectsMeasure<DeckCard>(Deck::EmProperty);
            return true;
        }();
        (void)registered;
        HorizontalAlignment(Layout::HorizontalAlignment::Stretch);
        VerticalAlignment(Layout::VerticalAlignment::Center);
        // Antialiased, alone among these controls: the panel's corner and its
        // stepped shadow read as a staircase drawn aliased.
        Media::RenderOptions::SetEdgeMode(*this, Media::EdgeMode::Antialias);
    }

    Size DeckCard::MeasureOverride(Size availableSize)
    {
        const double em = Em();
        const double pad = em * PadEms;
        const double limit = em * MaxWidthEms;
        // As a real MaxWidth, from the em width alone, so a narrow pass cannot
        // latch the panel narrow.
        if (std::abs(MaxWidth() - limit) > 0.01)
        {
            MaxWidth(limit);
        }
        const double cap = MathMin(availableSize.Width, limit);
        const Size inner{MathMax(0.0, cap - pad * 2), MathMax(0.0, availableSize.Height - pad * 2)};
        const Size child = Decorator::MeasureOverride(inner);
        // The cap, not the child's own width; the edge hangs below the face.
        if (Fill && !std::isinf(availableSize.Height))
        {
            return {cap, availableSize.Height};
        }
        return {cap, MathMin(availableSize.Height, child.Height + pad * 2)};
    }

    Size DeckCard::ArrangeOverride(Size finalSize)
    {
        const double pad = Em() * PadEms;
        if (const Controls::ControlPtr child = Child())
        {
            child->Arrange(Rect(pad, pad, MathMax(0.0, finalSize.Width - pad * 2),
                MathMax(0.0, finalSize.Height - pad * 2)));
        }
        return finalSize;
    }

    void DeckCard::Render(Media::DrawingContext& context)
    {
        const double w = Bounds().Width;
        const double h = Bounds().Height;
        if (w <= 0 || h <= 0)
        {
            return;
        }
        const double radius = Em() * 0.7;
        const RoundedRect face(Rect(0, 0, w, h), radius);
        // The reference's three shadows, in its own order: a two-point ring
        // (a spread, not a stroke), the lip, and the cast.
        const Media::BoxShadows shadows(Deck::Shadow(0, 0, 0, 2, Deck::Fade(0xe6eaf2, 0.18)),
            {Deck::Shadow(0, Lip, 0, 0, GuiTheme::PanelDeep), Deck::Shadow(0, 26, 50, 0, Deck::Fade(0, 0.75))});
        context.DrawRectangle(std::make_shared<Media::SolidColorBrush>(GuiTheme::Panel), nullptr, face, shadows);
    }
}
