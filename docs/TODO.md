# Graphics

* Graphics support should be enabled at compile-time with a macro
  definition, is it requires additional X libraries.  If not enabled,
  the program _must not_ depend on Xlib.

* Implement a `GRAPHICS` command
  * Takes either two positive numbers or the single number `0`.
    * If two positive numbers are given (in the range 1–16384), they
      are used as the _width_ and _height_ dimensions of a virtual
      image to use for drawing (replacing any existing image).  The
      image is shown in an X window initially scaled to roughly a
      third of the display size in the maximum dimension (keeping the
      aspect ratio).  The image starts out with a white background,
      the initial pen color is “Black” and the initial fill is `NONE`.
	* If the number `0` is given, it closes any existing X window.

* Implement drawing commands.  These commands only work while
  `GRAPHICS` is active; otherwise they result in an error.
  * `CLEAR` _color_ — Erases the entire image, setting the background
    to the given color (by name, using `XLookupColor(`…`)`.)
  * `PEN` _color_ — Sets the foreground color for other drawing commands
  * `FILL` _color_ _or_ `NONE` — Either sets the fill color for
    drawing solid shapes, or sets no fill.
  * `POINT` _x_`,` _y_ — Draw a single dot on the image (in the
    current pen color)
  * `LINE` _x<sub>1</sub>_`,` _y<sub>1</sub>_ `TO` _x<sub>2</sub>_`,`
    _y<sub>2</sub>_ — Draw a line on the image from the first
    coordinate to the second in the current pen color.  The
    coordinates may extend beyond the image boundaries, in which case
    they'll be clipped to the image border.
  * `RECTANGLE` _x<sub>1</sub>_`,` _y<sub>1</sub>_ `TO`
    _x<sub>2</sub>_`,` _y<sub>2</sub>_ — Draw a rectangle on the image
    with the given corners, having a border in the current pen color
    and interior possibly filled with the current fill color.
  * `CIRCLE` _x<sub>C</sub>_`,` _y<sub>C</sub> `RADIUS` _r_ — Draw a
    circle on the image centered at the given coordinate and having
    the given radius, having a border in the current pen color and
    interior possibly filled with the current fill color.  (This is
    effectively the same as `ELLIPSE` with the same _r_ given for both
    dimensions.)
  * `ELLIPSE` _x<sub>C</sub>_`,` _y<sub>C</sub> `RADII`
    _r<sub>x</sub>_`,` _r<sub>y</sub>_ — Draw an ellipse on the image
    centered at the given coordinate and having the given radii in the
    X and Y dimensions, having a border in the current pen color and
    interior possibly filled with the current fill color.
  * `POLYGON` _X_`,` _Y_ — Draw a polygon on the image using
    coordinates from two arrays.  The arrays _must_ be the same size.
    Since this BASIC interpreter accepts either 0-base or 1-base
    arrays, it uses the following heuristic to check which elements to
    use for the path (assuming the arrays were created by `DIM
    X(N)`):
	* If both coordinates at `[0]` and `[N]` are equal, it assumes
      this is an already-closed polygon and uses the full (N+1) array.
	* If one of the coordinates at `[0]` or `[N]` is (0, 0) but the
      other is not, it ignores the zero coordinate and uses the rest
      of the array.  If the last coordinate is _not_ the same as the
      first, it adds another copy of the first coordinate to close the
      polygon.
	* If both coordinates at `[0]` and `[N]` are non-zero and not
      equal, BASIC rejects the statement with an error saying that the
      array base is ambiguous.

_For a more advanced system, come up with a way to create “sprites” —
i.e. buffered images with transparency that may be overlaid on top of
the main image and moved around without disturbing it._
