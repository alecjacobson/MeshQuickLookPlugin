This is a small application which builds a "QuickLook generator" for Mac OS.
This will generate previews (selecting a file in Finder and hitting space) and
thumbnails ("Cover flow" view in Finder) for .obj, .off and .mesh 3D model
files.

Using [libigl][1] and Mesa3D's off screen rendering is was easy to whip up a
QuickLook preview and thumbnail generator for our favorite 3D model file
formats. I opted to avoid Xcode and compile the application using a good old
Makefile. You can find the project in the libigl examples
`libigl/examples/quicklook-mesh/` (as of version 0.3.2).

Once installed the QuickLook generator will make (static) previews of any
.mesh, .off, .obj or .wrl files when hit [space] after selecting the file in
Finder.app.

![Mesh quicklook preview][2]

Or when you use one of the fancier Finder.app views

![Mesh quicklook thumbnail][3]

I imagine I'll be perfecting this over time, but for now it shows 6 canonical
views and uses a double-sided material to give you an idea of any inconsistent
mesh orientation issues. It should work for triangle meshes or general
polygonal meshes.

# Binary 

You can skip compiling and just install the included binary, Mesh.qlgenerator. Install it by
moving `Mesh.qlgenerator/` into `/Library/QuickLook/`. Then either restart you
computer or issue in a Terminal:

    qlmanage -r
    qlmanage -r cache

# Binary

Included in this repository is a statically compiled, ready-to-use binary 

# Build

To build issue:

    make

This will compile the source and build the application in `./Mesh.qlgenerator/`

To install issue:

    sudo make install

This will copy the directory `./Mesh.qlgenerator/` into `/Library/QuickLook/` and
reload the Quicklook Manager (so that you see your changes in Finder).

# Dependencies

This plugin depends heavily on [libigl][1]

Install Mesa3D. For example, using macports:

    sudo port install mesa

Then re-install GLU à la http://www.alecjacobson.com/weblog/?p=2827

## Note about Mesa

If things look weird (too far away, blank, etc.) then maybe you should
reinstall GLU. It seems that the version of GLU that comes with macports' mesa
is buggy/corrupted.

# Testing

After installing you can test previews with:

    /usr/bin/qlmanage -p ../shared/cheburashka.off

and thumbnails with:

    /usr/bin/qlmanage -t ../shared/cheburashka.off
    
 [1]: https://github.com/libigl/libigl/
 [2]: http://alecjacobson.com/weblog/media/mesh-quicklook-preview-hand.jpg
 [3]: http://alecjacobson.com/weblog/media/mesh-quicklook-thumbnail-cover-flow.jpg
