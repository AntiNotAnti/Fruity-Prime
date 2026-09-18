using System;
using System.IO;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Threading;
using Avalonia.VisualTree;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// The one layout every screen in the launcher is built from.
    ///
    /// There used to be nine screens and each had picked its own answer to
    /// where things go: a card 400 wide over here, a 600-wide table over
    /// there, a bar across the top on one, a corner link on another. The
    /// screens were all the same product and none of them looked like it.
    ///
    /// So the answers live here and nowhere else -- the picture, the two
    /// washes over it, the column of words in the bottom-left corner, the line
    /// under it, and the two marks in the bottom corners that mean yes and no.
    /// A screen is a choice of what goes in those places, which is why there
    /// are four of them now instead of nine.
    ///
    /// The numbers are the front screen's, unchanged: it was the one screen
    /// that already read the way the rest were meant to.
    /// </summary>
    internal static class UiLayout
    {
        /// <summary>Where the column of words starts, and how far it clears the bottom edge.</summary>
        public const double ColumnLeft = 72;
        public const double ColumnBottom = 96;

        /// <summary>The corner marks, and the dim line under the column.</summary>
        public const double CornerX = 64;
        public const double CornerY = 38;
        public const double FooterBottom = 18;

        /// <summary>A menu word, and the one word a screen is named by.</summary>
        public const double WordSize = 20;
        public const double HeadingSize = 15;

        /// <summary>Where a screen's own content sits: clear of the tabs and of both corners.</summary>
        public static Thickness BodyMargin => new(ColumnLeft, 104, CornerX, 96);

        /// <summary>Where the tab strip sits: the top-left corner, above the body.</summary>
        public static Thickness TabMargin => new(ColumnLeft, 52, 0, 0);

        // ------------------------------------------------------- the well
        //
        // Everything behind the front screen is laid out in one column down
        // the middle of the frame, with the cross and the tick together at
        // its foot. The front screen is the exception and keeps its corner:
        // it is three words over a photograph and has nothing to hold.
        //
        // The well has a *fixed* width, which is the whole argument for it.
        // A layout that fills the window puts a settings row's label at one
        // edge and its control at the other, so on a wide screen the two ends
        // of one row are a foot apart and reading it means crossing the
        // monitor -- and the wider the display, the worse it gets. Here a
        // wider window gives the *photograph* more room and the content
        // exactly as much as it had, so a row is the same shape on a laptop
        // and on an ultrawide.
        //
        // The marks move for the same kind of reason. A cross in one corner
        // and a tick in the other are two things to find; side by side under
        // the content they are one thing to read, in the order they are read
        // in -- no, then yes -- and they sit directly under where the eye
        // already is rather than in the two places it is not.

        /// <summary>
        /// How wide the well is, per kind of screen.
        ///
        /// Chosen against the smallest layout box the surface will hand out
        /// (960 by 600 -- see <c>UiSurface.Factor</c>): a well as wide as that
        /// box is a well with no margin, which is a full-width layout wearing
        /// a centred heading. These leave room either side at every size the
        /// program allows.
        /// </summary>
        public const double WellPlay = 820;
        public const double WellSettings = 640;

        /// <summary>A question, a menu, a progress log: content, not a table.</summary>
        public const double WellShort = 480;

        /// <summary>What the well clears at the top, and at the foot for the marks.</summary>
        public const double WellTop = 44;
        public const double WellBottom = 84;

        /// <summary>
        /// The least ground either side of the well.
        ///
        /// The widths above are fixed on purpose and a wider box gives the
        /// photograph the difference, not the content -- but a box *narrower*
        /// than the well is a different question, and it has one answer: the
        /// well has to give way, or it is drawn off both edges of the screen.
        /// That is not a case the desktop reaches (the surface never hands out
        /// a box under 960 points and the widest well is 820), and it is the
        /// ordinary case on a phone, which is about 830 points across in
        /// landscape. So the width is a maximum rather than a size, and this
        /// is what is kept clear when it binds.
        /// </summary>
        public const double WellGutter = 20;

        /// <summary>Where the pair of marks sits, and how far apart.</summary>
        public const double MarksBottom = 28;
        public const double MarksGap = 64;

        /// <summary>
        /// The wash the well is read against: darkest through the middle band
        /// where the content is, fading out top and bottom so the photograph
        /// is still a photograph.
        ///
        /// Not a panel and not a card -- it has no edge anywhere, which is the
        /// one rule these screens have always kept. And it is never laid over
        /// a match: the scrim is already the ground there, and a second wash
        /// on top of it takes the match away, which is the thing the pause
        /// menu exists to keep visible.
        /// </summary>
        public static Border Wash(byte core = 228, byte edge = 120)
        {
            return new Border
            {
                Background = new LinearGradientBrush
                {
                    StartPoint = new RelativePoint(0.5, 0, RelativeUnit.Relative),
                    EndPoint = new RelativePoint(0.5, 1, RelativeUnit.Relative),
                    GradientStops =
                    {
                        new GradientStop(Color.FromArgb(edge, 10, 12, 16), 0),
                        new GradientStop(Color.FromArgb(core, 10, 12, 16), 0.16),
                        new GradientStop(Color.FromArgb(core, 10, 12, 16), 0.88),
                        new GradientStop(Color.FromArgb(edge, 10, 12, 16), 1)
                    }
                }
            };
        }

        /// <summary>
        /// The front screen's wash: the same gradient, a third of the weight.
        ///
        /// The default is set by the densest thing any screen has to carry --
        /// fourteen settings rows, which have to be legible over whatever the
        /// photograph is doing. The front screen carries three words, and
        /// three words need almost nothing; painting them against the settings
        /// page's ground would throw the picture away to solve a problem this
        /// screen does not have.
        /// </summary>
        public static Border LightWash() => Wash(core: 140, edge: 55);

        /// <summary>
        /// Which wash a backdrop carries, so that it can be baked into the
        /// same bitmap as the photograph under it.
        ///
        /// A wash is a full-window gradient with alpha and costs about what
        /// the photograph does to rasterise, so leaving it live would leave
        /// a third of <see cref="BakedBackdrop"/>'s saving on the table.
        /// </summary>
        public enum BackdropWash
        {
            /// <summary>Nothing over the photograph. The design studies, which draw their own.</summary>
            None,
            /// <summary>What every screen behind the front one is read against.</summary>
            Standard,
            /// <summary>The front screen's: three words need almost no ground.</summary>
            Light
        }

        /// <summary>
        /// How many device pixels one layout point is, for whoever is cutting
        /// a bitmap rather than drawing into the frame.
        ///
        /// <see cref="UiSurface"/> scales the whole screen with a layout
        /// transform, so a control's own <c>Bounds</c> are in points and
        /// nothing in the visual tree can see what those land on. The
        /// backdrop has to know: baked at the point size it would be blown up
        /// by this much and the photograph would be visibly soft. One is the
        /// right answer everywhere else -- the capture commands render at the
        /// size they are given, with no transform in the way.
        /// </summary>
        public static double BakeScale { get; set; } = 1;

        /// <summary>
        /// How much bigger than its own layout a box this tall draws the
        /// screens.
        ///
        /// Here rather than in <see cref="UiSurface"/>, where it was written,
        /// because it is not the desktop's rule -- it is the launcher's, and
        /// there are two heads. The desktop asks about the game window and
        /// scales the surface it composites; Android asks about the view the
        /// activity was given and scales that. A phone that skips this gets
        /// laid out in its own ~400 point height, which is a third of what
        /// every screen here is authored for: the well is wider than the
        /// screen, the server list is arranged off the side of it, and the
        /// result looks nothing like the same program.
        ///
        /// One rule for every screen -- the front screen, the pause menu, the
        /// settings, in a match or not. Two of them had different scales for a
        /// build and the difference is exactly what got reported: the menus in
        /// a match were readable and the front screen that came back when the
        /// match ended "went small again". A menu is a menu.
        ///
        /// It is not linear in the window's height, and that is deliberate.
        /// Straight proportion keeps text the same *fraction* of the picture,
        /// which is right for a HUD and wrong for something you read: a
        /// 1280x768 window sits an arm's length away on a desk, and a
        /// fullscreen 1080p or 4K picture is usually a bigger screen further
        /// off, wanting more than proportionally larger type. The exponent is
        /// what carries that -- a window twice as tall draws the screens about
        /// 2.8 times as large -- and 720 is the height at which they are drawn
        /// as they were authored.
        ///
        /// Both ends were reported, from the same build, in the same
        /// sentence: too big in the window it opens in, too small in
        /// fullscreen. The numbers below are the two anchors that came out of
        /// that -- about 1.1 at 1280x768, about 1.85 at 1080p.
        ///
        /// Eighth steps: a drag would otherwise relayout the whole screen on
        /// every pixel, and quarters were a visible jump at the boundary.
        ///
        /// The height is what the curve is drawn from, but it is not the only
        /// thing that decides: the screens also have to *fit*. A window wider
        /// than it is tall is the ordinary case and it is the one the height
        /// alone got wrong -- 1440p asked for 2.875, which leaves the screens
        /// 890 by 500 points to lay themselves out in, and they are authored
        /// for something near <see cref="MinBoxWidth"/> by <see cref="MinBoxHeight"/>. The type then looks
        /// enormous because everything around it has been squeezed, and the
        /// column of settings beside the list runs off the bottom of its own
        /// grid row and is drawn straight over the tick in the corner. So the
        /// curve is capped by what the window can actually hold, and the
        /// layout box never goes below the size the screens were drawn for.
        /// </summary>
        public static double Factor(double width, double height)
        {
            double room = Math.Max(height, 1) / 720.0;
            double raw = Math.Pow(room, 1.5) * Mods.Render.VisualOptions.Current.UiScale / 100.0;
            double fits = Math.Min(Math.Max(width, 1) / MinBoxWidth,
                Math.Max(height, 1) / MinBoxHeight);
            // The curve rounds to the nearest eighth and the cap rounds down
            // to one: a cap rounded to the nearest is a cap that can be
            // exceeded, which is the one thing it is there to stop.
            double stepped = Math.Min(Math.Round(raw * 8), Math.Floor(fits * 8)) / 8;
            // Down to 0.6, because the smallest window this program allows is
            // shorter than the space the screens are drawn in and the menu has
            // to fit inside it; up to four, past which nothing is legible for
            // a different reason.
            return Math.Clamp(stepped, 0.6, 4);
        }

        /// <summary>
        /// The smallest layout box the screens are allowed to be given, in
        /// their own points -- roughly the launcher window that used to open.
        ///
        /// It is the floor under <see cref="Factor"/> and nothing else: a
        /// window smaller than this in real pixels still gets the 0.6 clamp
        /// and a box smaller than this, because there is nothing else to do
        /// with it. A phone in landscape is exactly that case and is why the
        /// clamp matters on the other head: 829 by 393 points asks for 0.375
        /// off the curve and gets 0.6, which still fits a 1382 by 655 box.
        /// </summary>
        public const double MinBoxWidth = 960;
        public const double MinBoxHeight = 600;

        /// <summary>
        /// The column itself: what the screen is called, the strip of pages or
        /// sources under it, and the screen's own content under that.
        ///
        /// <paramref name="centreBody"/> for content that is shorter than the
        /// well -- a menu, a question. A list or a page of settings wants the
        /// height it is given, so it stretches.
        /// </summary>
        /// <summary>
        /// How short a box has to be before the well stops spending a third of
        /// it on its own margins.
        ///
        /// A desktop window is 600 points tall or more and 44 above plus 84
        /// below is a comfortable seventh of it. A phone in landscape is about
        /// 390 (see <c>UiScaleHost</c>), where the same two numbers are a
        /// third of the screen given to nothing -- and the thing they take it
        /// from is the list, which is what the screen is for.
        /// </summary>
        public const double ShortBox = 560;

        /// <summary>
        /// The well, which gives its margins back when there is no height to
        /// spare.
        ///
        /// A Grid that reads the height it is being measured with, because
        /// there is nowhere else to read it: the box comes from the window on
        /// one head and the activity's view on the other, and by the time this
        /// is built neither has said anything.
        /// </summary>
        private sealed class WellGrid : Grid
        {
            private readonly double _top;
            private readonly double _bottom;
            private bool _short;

            public WellGrid(double top, double bottom)
            {
                _top = top;
                _bottom = bottom;
                Margin = new Thickness(WellGutter, top, WellGutter, bottom);
            }

            protected override Size MeasureOverride(Size availableSize)
            {
                if (!Double.IsInfinity(availableSize.Height) && availableSize.Height > 0)
                {
                    // The full margin plus the height it is inside: what the
                    // box would have been. Compared against the threshold
                    // rather than the arrived-at height, or the answer would
                    // flip every time it changed.
                    bool tight = availableSize.Height + _top + _bottom < ShortBox;
                    if (tight != _short)
                    {
                        _short = tight;
                        // Still enough under the well for the marks (a mark is
                        // about 26 and sits MarksBottom off the floor); the
                        // rest of both numbers is breathing room, and a short
                        // screen has none to lend.
                        Margin = tight
                            ? new Thickness(WellGutter, 16, WellGutter,
                                _bottom > _top ? 58 : 16)
                            : new Thickness(WellGutter, _top, WellGutter, _bottom);
                    }
                }
                return base.MeasureOverride(availableSize);
            }
        }

        public static Grid Well(double width, string heading, Control? strip,
            Control body, bool centreBody = false, bool room = true)
        {
            var title = new TextBlock
            {
                Text = UiText.Sentence(heading),
                FontFamily = GuiTheme.Display,
                FontSize = HeadingSize,
                Foreground = GuiTheme.TextDimBrush,
                HorizontalAlignment = HorizontalAlignment.Center,
                Margin = new Thickness(0, 0, 0, strip == null ? 20 : 10),
                IsVisible = heading.Length > 0
            };
            if (strip != null)
            {
                strip.HorizontalAlignment = HorizontalAlignment.Center;
                strip.Margin = new Thickness(0, 0, 0, 22);
            }
            // The foot only has to clear the marks when there are any. A
            // screen with none -- the pause menu -- was being pushed into the
            // top half of the frame by a gap left for nothing.
            var well = new WellGrid(WellTop, room ? WellBottom : WellTop)
            {
                // A maximum, not a size, and stretched rather than centred:
                // stretch-with-a-maximum is the one combination that fills the
                // box up to the width asked for and centres what is left over,
                // which is "820 points wherever there is room for it and the
                // screen's width where there is not". See WellGutter.
                MaxWidth = width,
                HorizontalAlignment = HorizontalAlignment.Stretch
            };
            if (centreBody)
            {
                // The heading travels with the content rather than staying at
                // the top of the well. A menu centred in the frame under a
                // word pinned forty points above it does not read as one
                // thing, and a pause menu is one thing.
                var group = new StackPanel
                {
                    Spacing = 0,
                    VerticalAlignment = VerticalAlignment.Center
                };
                group.Children.Add(title);
                if (strip != null)
                {
                    group.Children.Add(strip);
                }
                group.Children.Add(body);
                well.Children.Add(group);
                return well;
            }
            well.RowDefinitions = new RowDefinitions("Auto,Auto,*");
            Grid.SetRow(title, 0);
            well.Children.Add(title);
            if (strip != null)
            {
                Grid.SetRow(strip, 1);
                well.Children.Add(strip);
            }
            Grid.SetRow(body, 2);
            well.Children.Add(body);
            return well;
        }

        /// <summary>
        /// The pair of marks at the foot, in reading order: no on the left,
        /// yes on the right, together rather than in opposite corners.
        /// </summary>
        public static StackPanel Marks(params UiMark?[] marks)
        {
            var row = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                Spacing = MarksGap,
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Bottom,
                Margin = new Thickness(0, 0, 0, MarksBottom)
            };
            foreach (UiMark? mark in marks)
            {
                if (mark == null)
                {
                    continue;
                }
                mark.HorizontalAlignment = HorizontalAlignment.Left;
                mark.VerticalAlignment = VerticalAlignment.Center;
                row.Children.Add(mark);
            }
            return row;
        }

        /// <summary>
        /// A whole screen: the backdrop, the wash, the well and the marks, in
        /// that order.
        ///
        /// Every screen behind the front one is built from this and nothing
        /// else, so none of them can invent its own answer to where a heading
        /// goes -- which is what nine screens with nine layouts was, and what
        /// this file exists to stop happening again.
        ///
        /// Extra things a screen needs in the frame rather than in the well --
        /// a line of status under the marks, say -- are added to the returned
        /// panel afterwards.
        /// </summary>
        /// <param name="extra">
        /// A third mark, between the two, for a screen whose foot carries an
        /// act that is neither leaving nor committing -- creating a server on
        /// the browser, say. Deliberately awkward to reach: the pair is the
        /// rule, and a screen that wants a third has to say so.
        /// </param>
        public static Panel Page(bool overGame, double width, string heading,
            Control? strip, Control body, UiMark? no = null, UiMark? yes = null,
            bool centreBody = false, UiMark? extra = null)
        {
            // The wash goes into the backdrop rather than over it: baked
            // together they are one blit a frame instead of four full-window
            // rasterisations. See BakedBackdrop for what that was costing.
            Panel root = Backdrop(overGame,
                overGame ? BackdropWash.None : BackdropWash.Standard);
            root.SetValue(ControllerNav.NavScopeProperty, "page");
            if (no != null) ControllerNav.Identify(no, "page.back");
            if (yes != null) ControllerNav.Identify(yes, "page.accept");
            if (extra != null) ControllerNav.Identify(extra, "page.extra");
            if (no != null && yes != null)
            {
                no.SetValue(ControllerNav.NavRightProperty, "page.accept");
                yes.SetValue(ControllerNav.NavLeftProperty, "page.back");
            }
            root.Children.Add(Well(width, heading, strip, body, centreBody,
                room: no != null || yes != null || extra != null));
            if (no != null || yes != null || extra != null)
            {
                root.Children.Add(Marks(no, extra, yes));
            }
            return root;
        }

        private static readonly Lazy<Bitmap?> _background =
            new(() => Load("Backgrounds/launcher-bg.jpg"));

        private static readonly Lazy<Bitmap?> _wordmark =
            new(() => Load("fruity-prime-logo.png"));

        private static Bitmap? Load(string asset)
        {
            try
            {
                using Stream stream = AssetLoader.Open(
                    new Uri($"avares://FruityPrime/Assets/{asset}"));
                return new Bitmap(stream);
            }
            catch (Exception)
            {
                // A build missing the asset gets no picture rather than no
                // launcher.
                return null;
            }
        }

        /// <summary>
        /// What every screen is painted on: the photo, a wash that carries the
        /// left third dark enough to read a menu over, and a corner vignette
        /// for the wordmark.
        ///
        /// Over a match it is the scrim alone. The match behind is still
        /// running -- a networked one cannot be paused -- and covering it with
        /// a photograph would be a lie about what the program is doing.
        /// </summary>
        public static Panel Backdrop(bool overGame = false,
            BackdropWash wash = BackdropWash.None)
        {
            var root = new Panel { Tag = "UiAccessibilityRoot" };
            root.Children.Add(overGame ? new Border { Background = GuiTheme.ScrimBrush } : new BakedBackdrop(wash));
            var contrast = new Border { Background = GuiTheme.InkBrush, IsHitTestVisible = false,
                IsVisible = Mods.Render.VisualOptions.Current.HighContrast };
            root.Children.Add(contrast);
            double textFactor = UiMetrics.TextFactor;
            void RefreshAccessibility()
            {
                contrast.IsVisible = Mods.Render.VisualOptions.Current.HighContrast;
                double ratio = UiMetrics.TextFactor / textFactor;
                foreach (var control in root.GetVisualDescendants().OfType<Control>())
                {
                    // A front screen can contain another backdrop in its overlay.
                    // Each scope updates only its own labels, avoiding double scaling.
                    if (control.GetVisualAncestors().OfType<Panel>().FirstOrDefault(panel =>
                        Equals(panel.Tag, "UiAccessibilityRoot")) != root) continue;
                    // Explicit sizes need updating on surviving screens when Settings closes.
                    if (control is TextBlock text && text.IsSet(TextBlock.FontSizeProperty)) text.FontSize *= ratio;
                    if (control is TextBox box && box.IsSet(TextBox.FontSizeProperty)) box.FontSize *= ratio;
                    control.InvalidateMeasure(); control.InvalidateVisual();
                }
                textFactor = UiMetrics.TextFactor;
                root.InvalidateMeasure();
            }
            void Changed() => Dispatcher.UIThread.Post(RefreshAccessibility);
            root.AttachedToVisualTree += (_, _) => { RefreshAccessibility(); Mods.Render.VisualOptions.Changed += Changed; };
            root.DetachedFromVisualTree += (_, _) => Mods.Render.VisualOptions.Changed -= Changed;
            return root;
        }

        /// <summary>
        /// The layers themselves, for <see cref="BakedBackdrop"/> to render
        /// once into the bitmap everything else draws.
        ///
        /// This is the backdrop as it has always been -- it is only ever
        /// rasterised now instead of being kept in the tree.
        /// </summary>
        public static Panel BackdropLayers(BackdropWash wash)
        {
            var root = new Panel();
            bool photoBelow = false;
#if MPHREAD_SHELL
            photoBelow = Mods.Render.LauncherPhoto.Enabled;
#endif
            if (!photoBelow)
            {
                // Only where nothing else is drawing it. The desktop shell
                // puts the photograph on the screen as a GL quad at the
                // window's own resolution -- see LauncherPhoto -- because this
                // bitmap is capped at 1080p and magnified, and a photograph is
                // the one layer that shows it. The washes below stay here
                // either way: they are gradients, and a gradient magnifies for
                // nothing.
                root.Children.Add(new Image
                {
                    Source = _background.Value,
                    Stretch = Stretch.UniformToFill
                });
            }
            root.Children.Add(new Border
            {
                Background = new LinearGradientBrush
                {
                    GradientStops =
                    {
                        new GradientStop(Color.FromArgb(215, 0, 0, 0), 0),
                        new GradientStop(Color.FromArgb(110, 0, 0, 0), 0.38),
                        new GradientStop(Color.FromArgb(0, 0, 0, 0), 0.68)
                    }
                }
            });
            root.Children.Add(new Border
            {
                Background = new RadialGradientBrush
                {
                    Center = new RelativePoint(1, 1, RelativeUnit.Relative),
                    GradientOrigin = new RelativePoint(1, 1, RelativeUnit.Relative),
                    RadiusX = new RelativeScalar(0.5, RelativeUnit.Relative),
                    RadiusY = new RelativeScalar(0.7, RelativeUnit.Relative),
                    GradientStops =
                    {
                        new GradientStop(Color.FromArgb(150, 0, 0, 0), 0),
                        new GradientStop(Color.FromArgb(0, 0, 0, 0), 1)
                    }
                }
            });
            if (wash == BackdropWash.Standard)
            {
                root.Children.Add(Wash());
            }
            else if (wash == BackdropWash.Light)
            {
                root.Children.Add(LightWash());
            }
            return root;
        }

        /// <summary>The column of words, anchored where the front screen's is.</summary>
        public static StackPanel Column(double spacing = 16)
        {
            return new StackPanel
            {
                Spacing = spacing,
                HorizontalAlignment = HorizontalAlignment.Left,
                VerticalAlignment = VerticalAlignment.Bottom,
                Margin = new Thickness(ColumnLeft, 0, 0, ColumnBottom)
            };
        }

        /// <summary>
        /// The dim line under the column: the build on the front screen, what
        /// match you are in on the pause screen. One line, one corner, and
        /// never anything a player came here to read.
        /// </summary>
        public static TextBlock Footer(string text)
        {
            return new TextBlock
            {
                Text = text,
                FontFamily = GuiTheme.Display,
                FontSize = 12,
                Foreground = GuiTheme.TextDimBrush,
                HorizontalAlignment = HorizontalAlignment.Left,
                VerticalAlignment = VerticalAlignment.Bottom,
                Margin = new Thickness(ColumnLeft - 50, 0, 0, FooterBottom)
            };
        }

        /// <summary>The mark in the opposite corner, at the size the front screen uses.</summary>
        public static Image Wordmark()
        {
            return new Image
            {
                Source = _wordmark.Value,
                Stretch = Stretch.Uniform,
                Width = 220,
                HorizontalAlignment = HorizontalAlignment.Right,
                VerticalAlignment = VerticalAlignment.Bottom,
                Margin = new Thickness(0, 0, 32, 28),
                Opacity = 0.92
            };
        }

        /// <summary>
        /// What a screen is called, in the top-left corner over the tabs.
        /// Lower case and dim on purpose: it says where you are, and a title
        /// that shouts is a title competing with the thing it titles.
        /// </summary>
        public static TextBlock Heading(string text)
        {
            return new TextBlock
            {
                Text = UiText.Sentence(text),
                FontFamily = GuiTheme.Display,
                FontSize = HeadingSize,
                Foreground = GuiTheme.TextDimBrush,
                HorizontalAlignment = HorizontalAlignment.Left,
                VerticalAlignment = VerticalAlignment.Top,
                Margin = new Thickness(ColumnLeft, 26, 0, 0)
            };
        }
    }
}
