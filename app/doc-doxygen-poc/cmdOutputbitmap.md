# Export to Bitmap {#cmdOutputbitmap}

![Export to Bitmap](bexportbmap.png)

This menu item creates a bitmap file of the layout.

![Bitmap dialog](bitmap.png)

The **Bitmap** dialog specifies the style and size of the bitmap. The check-boxes
(**Layout Titles**, **Borders**, **Centerline of Track** and **Background Image**)
control whether the Layout Title, the borders or the track centerlines are printed
on the bitmap. If a background image is used it will be printed if the option is
set.

The size of the bitmap is smaller if these are disabled.

Printing the track centerlines (also seen when zoomed in 1:1) is useful when you
later print the bitmap full size for laying out track.

The **DPI** control specifies the number of pixels per inch in the bitmap. Bitmaps
must be less than 32,000 pixels in height or width. The upper value you can enter
depends on the size of your trackplan. It is made sure that your bitmap does not
exceed these limits. Larger values will result in a larger bitmap file.

The bitmap width, height and approximate file size is indicated.

@note This command can create a very large file and consume a lot of memory and
time.

Pressing the **OK** button invokes a **File Save** dialog so you can choose the
file name for the Bitmap.

You can select to create JPEG or PNG files. As a rule of thumb JPEG results in
smaller files when you use a background image, PNG does so for trackplans without
an image background.
