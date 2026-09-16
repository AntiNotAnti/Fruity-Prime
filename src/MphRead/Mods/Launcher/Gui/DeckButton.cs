#if MPHREAD_AVALONIA
using System;
using System.Collections.Generic;
using System.Diagnostics;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// A button with a thickness.
    ///
    /// <para>
    /// The whole of the "deck" look is in one detail, and it is not the
    /// colour: a solid, unblurred edge below the face, in the face's own hue
    /// two stops down. That is what makes the control read as an object with a
    /// depth rather than a rectangle with a gradient, and it is why the press
    /// works -- the face travels down by exactly the edge it loses, so it
    /// lands where the edge's bottom was. Anything else is a rectangle moving.
    /// </para>
    ///
    /// <para>
    /// Every measurement is an em. The font size is in the <i>frame's</i> ems
    /// (<see cref="Deck.EmProperty"/>) and the padding and the radius are in
    /// the <i>button's own</i>, which is exactly how the reference nests them:
    /// <c>font-size: 1.05em</c> on the tab, then <c>padding: .38em .8em</c>
    /// against that. Getting the two levels the wrong way round is how a tab
    /// strip comes out the size of a menu entry.
    /// </para>
    ///
    /// <para>
    /// Avalonia has no CSS transitions and this codebase paints its own
    /// controls, so the motion is a spring integrated in <see cref="Render"/>:
    /// a position, a velocity, a stiffness and a damping, stepped by the time
    /// since the last frame and re-invalidating until it settles. That gives
    /// the overshoot a cubic-bezier cannot, costs one float of state, and
    /// stops drawing the moment it stops moving -- which matters here, because
    /// these are composited into the game window every frame it draws.
    /// </para>
    ///
    /// <para>
    /// The hovered face also leans toward the pointer. Seven degrees is where
    /// it stops reading as a card and starts reading as a wobble; a bare
    /// rotation is the whole of it, since a real perspective divide on a
    /// control this size is invisible next to the cost of doing it.
    /// </para>
    /// </summary>
    internal sealed class DeckButton : Control
    {
        public event EventHandler? Click;

        private string _text;
        private Deck.Face _face;
        private bool _selected;

        /// <summary>Font size, in the frame's ems.</summary>
        private readonly double _sizeEms;

        /// <summary>Padding and radius, in this button's own ems.</summary>
        private readonly double _padXEms, _padYEms;

        /// <summary>The edge, in points. Flat, like the reference's <c>--lip</c>.</summary>
        private readonly double _lip;

        /// <summary>
        /// The key that does the same thing, in a chip on the face.
        ///
        /// A mark used to carry a drawn glyph -- a tick, a cross, a plus --
        /// which said what kind of thing it was and nothing a player did not
        /// already know from the word beside it, at four times the width. The
        /// chip says something they cannot know instead.
        /// </summary>
        public string KeyCap { get; set; } = "";

        /// <summary>
        /// The reference's <c>.btn.idle</c>: two and a half points of bob on a
        /// 3.4 second cycle, on the one button the screen wants looked at.
        ///
        /// It is off by default and has to be asked for, because it is the one
        /// thing on these screens that redraws when nothing has happened --
        /// and a redraw here costs the whole launcher surface. One button on
        /// one screen is the budget.
        /// </summary>
        public bool Idle { get; set; }

        private readonly Tap _tap = new();

        private const double LipPressed = 2;
        private const double RadiusEms = 0.55;
        private const double MaxTilt = 7;

        /// <summary>Where the spring is, where it is going, and how fast.</summary>
        private double _pop = 1, _popVelocity;
        private double _popTarget = 1;
        private double _tilt, _tiltTarget;
        private readonly Stopwatch _clock = Stopwatch.StartNew();
        private TimeSpan _last;

        /// <summary>
        /// Stiffness and damping. 220 and 18 overshoot by about six per cent
        /// and settle inside a fifth of a second, which is Balatro's pop as
        /// near as watching it frame by frame can place it.
        /// </summary>
        private const double Stiffness = 220;
        private const double Damping = 18;

        static DeckButton()
        {
            AffectsRender<DeckButton>(IsEnabledProperty);
            AffectsMeasure<DeckButton>(Deck.EmProperty);
            AffectsRender<DeckButton>(Deck.EmProperty);
        }

        public DeckButton(string text, Deck.Face face, double sizeEms = 1.55,
            double padXEms = 1.5, double padYEms = 0.85, double lip = 6)
        {
            _text = text;
            _face = face;
            _sizeEms = sizeEms;
            _padXEms = padXEms;
            _padYEms = padYEms;
            _lip = lip;
            Focusable = true;
            Cursor = new Cursor(StandardCursorType.Hand);
            // Pixel perfect: no edge feathering, so the face and the type land
            // on the same grid. Fully qualified: the engine has its own
            // RenderOptions and it wins the name here.
            Avalonia.Media.RenderOptions.SetEdgeMode(this, EdgeMode.Aliased);
        }

        /// <summary>The label, for a button whose word changes with the face it is on.</summary>
        public string Text
        {
            get => _text;
            set
            {
                if (_text == value)
                {
                    return;
                }
                _text = value;
                InvalidateMeasure();
                InvalidateVisual();
            }
        }

        /// <summary>
        /// Change face without rebuilding. A tab strip swaps colours on every
        /// press, and a control rebuilt on every press loses the spring it was
        /// in the middle of.
        /// </summary>
        public void Wear(Deck.Face face, bool selected)
        {
            _face = face;
            _selected = selected;
            InvalidateVisual();
        }

        /// <summary>Whether this is the tab that is up: the wedge over it says so.</summary>
        public bool Selected
        {
            get => _selected;
            set
            {
                if (_selected == value)
                {
                    return;
                }
                _selected = value;
                InvalidateVisual();
            }
        }

        /// <summary>This button's own em: the frame's, times its font size.</summary>
        private double Size => Deck.GetEm(this) * _sizeEms;

        private double KeyGap => Size * 0.5;

        private double KeyWidth(double size)
        {
            if (KeyCap.Length == 0)
            {
                return 0;
            }
            double cap = size * 0.5;
            double text = DeckText.Run(KeyCap, Deck.Body(bold: true), cap,
                GuiTheme.AccentBrush).Width;
            return Math.Max(cap * 1.5, text + cap * 0.7);
        }

        protected override Size MeasureOverride(Size availableSize)
        {
            double size = Size;
            double width = DeckText.MeasureTracked(_text, Deck.Label(), size,
                DeckText.LabelTracking) + size * _padXEms * 2;
            if (KeyCap.Length > 0)
            {
                width += KeyGap + KeyWidth(size);
            }
            double line = DeckText.Run("Hg", Deck.Label(), size, GuiTheme.TextBrush).Height;
            // The face alone. The edge is a `box-shadow`, and a shadow is
            // outside the box: a row of buttons lines up on the bottom of the
            // *faces* and the edges hang into the gap under them. Counting the
            // edge here instead pushed everything below a tab strip down by
            // six points and everything below a foot by another six, which is
            // most of what "close, but everything is a bit low" was.
            return new Size(Math.Min(Math.Round(width), availableSize.Width),
                Math.Round(line + size * _padYEms * 2));
        }

        protected override void OnPointerEntered(PointerEventArgs e)
        {
            _popTarget = 1.055;
            InvalidateVisual();
            base.OnPointerEntered(e);
        }

        protected override void OnPointerExited(PointerEventArgs e)
        {
            _popTarget = 1;
            _tiltTarget = 0;
            InvalidateVisual();
            base.OnPointerExited(e);
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            if (_tap.Down && _tap.Moved(e, this))
            {
                InvalidateVisual();
            }
            if (IsPointerOver)
            {
                double half = Bounds.Width / 2;
                if (half > 0)
                {
                    double dx = (e.GetPosition(this).X - half) / half;
                    _tiltTarget = Math.Clamp(dx, -1, 1) * MaxTilt;
                    InvalidateVisual();
                }
            }
            base.OnPointerMoved(e);
        }

        protected override void OnPointerPressed(PointerPressedEventArgs e)
        {
            _tap.Press(e, this);
            Focus();
            e.Pointer.Capture(this);
            e.Handled = true;
            InvalidateVisual();
            base.OnPointerPressed(e);
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            bool tapped = _tap.Release(e, this);
            InvalidateVisual();
            if (tapped)
            {
                e.Handled = true;
                Click?.Invoke(this, EventArgs.Empty);
            }
            base.OnPointerReleased(e);
        }

        protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs e)
        {
            _tap.Cancel();
            _popTarget = 1;
            _tiltTarget = 0;
            InvalidateVisual();
            base.OnPointerCaptureLost(e);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Enter || e.Key == Key.Space)
            {
                e.Handled = true;
                Click?.Invoke(this, EventArgs.Empty);
                return;
            }
            base.OnKeyDown(e);
        }

        protected override void OnGotFocus(GotFocusEventArgs e)
        {
            _popTarget = 1.055;
            InvalidateVisual();
            base.OnGotFocus(e);
        }

        protected override void OnLostFocus(RoutedEventArgs e)
        {
            if (!IsPointerOver)
            {
                _popTarget = 1;
            }
            InvalidateVisual();
            base.OnLostFocus(e);
        }

        /// <summary>
        /// Step the spring by real time rather than by frame, so the motion is
        /// the same whether this is drawn at 60Hz in the game window or once
        /// by <see cref="UiCapture"/>. A capture therefore lands on the
        /// resting pose, which is the one worth photographing -- and the idle
        /// bob starts at zero for the same reason.
        /// </summary>
        private bool Settle()
        {
            TimeSpan now = _clock.Elapsed;
            double dt = Math.Min(0.05, (now - _last).TotalSeconds);
            _last = now;
            if (Deck.Still)
            {
                _pop = _popTarget;
                _popVelocity = 0;
                _tilt = _tiltTarget;
                return false;
            }
            if (dt <= 0)
            {
                return false;
            }
            double accel = (_popTarget - _pop) * Stiffness - _popVelocity * Damping;
            _popVelocity += accel * dt;
            _pop += _popVelocity * dt;
            _tilt += (_tiltTarget - _tilt) * Math.Min(1, dt * 14);
            bool moving = Math.Abs(_popTarget - _pop) > 0.0005
                || Math.Abs(_popVelocity) > 0.0005
                || Math.Abs(_tiltTarget - _tilt) > 0.05;
            if (!moving)
            {
                _pop = _popTarget;
                _popVelocity = 0;
                _tilt = _tiltTarget;
            }
            return moving;
        }

        /// <summary>The bob, in points. Zero unless this is the one idle button.</summary>
        private double Bob()
        {
            if (!Idle || Deck.Still || IsPointerOver || IsFocused)
            {
                return 0;
            }
            // `@keyframes bob { 0%,100% { --ty: 0 } 50% { --ty: -2.5px } }`
            // over 3.4s, eased. A raised cosine is the same curve to the eye
            // and is one call.
            double phase = _clock.Elapsed.TotalSeconds % 3.4 / 3.4;
            return -1.25 * (1 - Math.Cos(phase * Math.PI * 2));
        }

        public override void Render(DrawingContext context)
        {
            bool moving = Settle();
            bool down = _tap.Down;
            bool on = IsEnabled;

            double lip = down ? LipPressed : _lip;
            // The face drops by exactly what the lip loses, so its bottom edge
            // stays where the lip's bottom edge was.
            double drop = _lip - lip + Bob();

            double w = Bounds.Width;
            double h = Bounds.Height;
            if (w <= 0 || h <= 0)
            {
                return;
            }

            double size = Size;
            double radius = size * RadiusEms;
            double scale = down ? 1 : _pop;
            Color fill = _face.Fill, lipColour = _face.Lip;
            if (!on)
            {
                // `filter: grayscale(.7) brightness(.55)`.
                fill = DeckPaint.Brightness(DeckPaint.Saturate(fill, 0.3), 0.55);
                lipColour = DeckPaint.Brightness(DeckPaint.Saturate(lipColour, 0.3), 0.55);
            }
            else if (IsPointerOver || IsFocused)
            {
                // `filter: brightness(1.22) saturate(1.15)`. Not a blend
                // towards white: that washes the hue out, and the hue is what
                // says which of five things the button is.
                fill = DeckPaint.Saturate(DeckPaint.Brightness(fill, 1.22), 1.15);
                lipColour = DeckPaint.Saturate(DeckPaint.Brightness(lipColour, 1.22), 1.15);
            }

            using (context.PushTransform(
                Avalonia.Matrix.CreateTranslation(-w / 2, -h / 2)
                * Avalonia.Matrix.CreateScale(scale, scale)
                * Avalonia.Matrix.CreateRotation(_tilt * Math.PI / 180 * 0.06)
                * Avalonia.Matrix.CreateTranslation(w / 2, h / 2 + drop)))
            {
                var faceRect = new RoundedRect(new Rect(0, 0, w, h), radius);

                // The wedge over the tab that is up, drawn under the face so
                // it reads as part of it: the strip has no other way to say
                // which page is showing once the dots and the arrows are gone.
                // `.tab[aria-selected]::before`, a .4em triangle .72em above.
                if (_selected)
                {
                    double half = size * 0.4;
                    double top = -size * 0.72;
                    var wedge = new StreamGeometry();
                    using (StreamGeometryContext g = wedge.Open())
                    {
                        double mid = Math.Round(w / 2);
                        g.BeginFigure(new Point(mid - half, top), true);
                        g.LineTo(new Point(mid + half, top));
                        g.LineTo(new Point(mid, top + half));
                        g.EndFigure(true);
                    }
                    context.DrawGeometry(new SolidColorBrush(fill), null, wedge);
                }

                // The face and its shadows, exactly as the reference declares
                // them: `0 var(--lip) 0 var(--face-lip), 0 calc(var(--lip) +
                // 4px) 12px rgba(0,0,0,.55)`, with `0 0 0 2px var(--accent)`
                // in front of both while it has the keyboard.
                //
                // The edge is a box-shadow there and is one here. It used to
                // be a second rounded rect drawn behind the face and the cast
                // was three translucent rects stepped outward, which is a fair
                // likeness of a blur and the wrong object for a ring: a `0 0 0
                // 2px` ring is a *spread*, and drawing it as a stroke puts
                // half of it inside the face and takes two points off the
                // label.
                var cast = new List<BoxShadow>
                {
                    Deck.Shadow(0, lip, 0, 0, lipColour),
                    Deck.Shadow(0, lip + 4, 12, 0, Deck.Fade(0, 0.55))
                };
                BoxShadow first = IsFocused
                    ? Deck.Shadow(0, 0, 0, 2, GuiTheme.Accent)
                    : cast[0];
                if (IsFocused)
                {
                    context.DrawRectangle(new SolidColorBrush(fill), null, faceRect,
                        new BoxShadows(first, cast.ToArray()));
                }
                else
                {
                    context.DrawRectangle(new SolidColorBrush(fill), null, faceRect,
                        new BoxShadows(first, new[] { cast[1] }));
                }

                // One hairline of light along the top, not a gradient over the
                // whole face: a gradient makes it a web button.
                var bevel = new LinearGradientBrush
                {
                    StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
                    EndPoint = new RelativePoint(0, 1, RelativeUnit.Relative),
                    GradientStops =
                    {
                        new GradientStop(Color.FromArgb(0x17, 255, 255, 255), 0),
                        new GradientStop(Color.FromArgb(0, 255, 255, 255), 1)
                    }
                };
                context.DrawRectangle(bevel, null,
                    new RoundedRect(new Rect(1, 1, w - 2, h * 0.4), radius - 1));

                IBrush ink = on ? GuiTheme.TextBrush : GuiTheme.TextDimBrush;
                double keyWidth = KeyWidth(size);
                double labelWidth = DeckText.MeasureTracked(_text, Deck.Label(), size,
                    DeckText.LabelTracking);
                double content = labelWidth + (keyWidth > 0 ? KeyGap + keyWidth : 0);
                double x = Math.Round((w - content) / 2);
                DeckText.DrawTracked(context, _text, Deck.Label(), size, ink, x, 0, h,
                    DeckText.LabelTracking);
                if (keyWidth > 0 && on)
                {
                    DeckChip.DrawKey(context, KeyCap, size,
                        Math.Round(x + labelWidth + KeyGap), h);
                }
            }

            // Not while the screen is being photographed: a control that asks
            // for another frame from inside a render pass is one a
            // RenderTargetBitmap refuses to finish. See Deck.Still.
            if ((moving || Idle) && !Deck.Still)
            {
                InvalidateVisual();
            }
        }
    }

    /// <summary>
    /// The two filters the reference puts on a hovered or a disabled face,
    /// done as arithmetic rather than as a blend towards a colour.
    ///
    /// <c>Shade</c> -- what this replaces -- moves a colour towards white,
    /// which lightens and desaturates at once. The reference brightens
    /// <i>and</i> saturates, so brass gets warmer rather than paler, and on a
    /// screen with five faces on it the hue is what says which button is which.
    /// </summary>
    internal static class DeckPaint
    {
        public static Color Brightness(Color c, double k)
        {
            return Color.FromArgb(c.A, Clamp(c.R * k), Clamp(c.G * k), Clamp(c.B * k));
        }

        /// <summary>The sRGB luma matrix, which is what a CSS saturate() filter is.</summary>
        public static Color Saturate(Color c, double s)
        {
            double luma = c.R * 0.2126 + c.G * 0.7152 + c.B * 0.0722;
            return Color.FromArgb(c.A,
                Clamp(luma + (c.R - luma) * s),
                Clamp(luma + (c.G - luma) * s),
                Clamp(luma + (c.B - luma) * s));
        }

        private static byte Clamp(double v) => (byte)Math.Clamp(v, 0, 255);
    }
}
#endif
