
INCLUDES = -I. -I../player \
        @XML2_CFLAGS@ @PTHREAD_CFLAGS@ @PANGOFT2_CFLAGS@ @PYTHON_INCLUDES@ \
        @DC1394_2_CFLAGS@ @LIBRSVG_CFLAGS@ @FONTCONFIG_CFLAGS@

ALL_H = WrapHelper.h raw_constructor.hpp

if APPLE
    XGL_LIBS =
    APPLE_LDFLAGS=-read_only_relocs suppress -F/System/Library/PrivateFrameworks -framework MultitouchSupport 
else
    XGL_LIBS = -lXxf86vm
    APPLE_LDFLAGS=
endif

ALL_GL_LIBS = @GL_LIBS@ @SDL_LIBS@ $(XGL_LIBS)

pkgpyexec_LTLIBRARIES = avg.la
avg_la_SOURCES = WrapHelper.cpp avg_wrap.cpp node_wrap.cpp raster_wrap.cpp \
        event_wrap.cpp fx_wrap.cpp \
        device_wrap.cpp bitmap_wrap.cpp anim_wrap.cpp $(ALL_H) 
avg_la_LDFLAGS = $(APPLE_LDFLAGS) -module -XCClinker
avg_la_LIBADD = ../conradrelais/libconradrelais.la \
                ../player/libplayer.la \
                ../audio/libaudio.la \
                ../video/libvideo.la \
                ../graphics/libgraphics.la \
                ../base/libbase.la \
                ../base/triangulate/libtriangulate.la \
                ../imaging/libimaging.la \
                ../anim/libanim.la \
                ../lmfit/liblmfit.la \
                ../oscpack/liboscpack.la \
                @PANGOFT2_LIBS@ @LIBRSVG_LIBS@ $(BOOST_PYTHON_LIBS)\
                -l@BOOST_THREAD_LIB@ @XML2_LIBS@ \
                @DC1394_2_LIBS@ @GLU_LIBS@ $(XI2_1_LIBS) $(XI2_2_LIBS) \
                $(ALL_GL_LIBS) @LIBFFMPEG@ @PTHREAD_LIBS@ \
                @GDK_PIXBUF_LIBS@ @FONTCONFIG_LIBS@

