###############################################################################
# flags used to compile this facedetect
# add other _CFLAGS and _LIBS as needed
#
# override CV_INLINE: GLib will take care of defining 'inline' sufficiently and
# OpenCV's define isn't good enough to avoid 'unused' gcc warnings (at v2.1.0)
TS_OCV_SOURCES =           opencv/gstpyrlk.c                           \
                           opencv/opencv_functions.c                   \
                           opencv/gstskin.c                            \
                           opencv/gstcontours.c                        \
                           opencv/gstdilate.c                          \
                           opencv/gsterode.c                           \
                           opencv/gstmorphology.c                      \
                           opencv/gsthisteq.c                          \
                           opencv/gstcodebookfgbg.c                    \
                           ssim/ssim.c   ssim/gstssim2.c               \
                           vibe/vibe.c  vibe/gstvibe.c                 \
                           opencv/gstalphamix.c                        \
                           opencv/gstgcs.c                             \
                           opencv/grabcut_wrapper.cpp                  \
                           opencv/gstgc.c                              \
                           opencv/gstsnakes.c                          \
                           opencv/gstdraweventbox.c                    \
                           retinex/gstretinex.c                        \
                           retinex/retinex.c                           
                           #opencv/gsttsm.c                             

TS_OCV_CFLAGS = -DCV_INLINE="static inline" -DCV_NO_BACKWARD_COMPATIBILITY

#TS_OCV_CFLAGS = $(GST_BASE_CFLAGS)                                     \
#                $(GST_PLUGINS_BASE_CFLAGS) -DCV_INLINE="static inline" \
#               -DCV_NO_BACKWARD_COMPATIBILITY                          \
#                $(OPENCV_CFLAGS)                                       \
#               -I/usr/include/opencv                                   

#TS_OCV_LIBS=  $(OPENCV_LIBS)                                           \
#            -lopencv_core -lopencv_highgui -lopencv_ml -lopencv_objdetect -lopencv_contrib \
#            -lavcodec -lavutil -lswscale -lpng -lgstvideo-0.10

#TS_OCV_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(OPENCV_LIBS) \
#                      $(GSTPB_BASE_LIBS)

###############################################################################

GSTFACETRACKER_SOURCES = gstfacetracker/camshift.cpp        \
                         gstfacetracker/colorbins.cpp       \
                         gstfacetracker/drawing.cpp         \
                         gstfacetracker/image.cpp           \
                         gstfacetracker/face.cpp            \
                         gstfacetracker/facedetection.cpp   \
                         gstfacetracker/ftrack.cpp          \
                         gstfacetracker/helper.cpp          \
                         gstfacetracker/haarclass.cpp       \
                         gstfacetracker/imagefeature.cpp    \
                         gstfacetracker/kalman.cpp          \
                         gstfacetracker/mtxcore.cpp         \
                         gstfacetracker/gstfacetracker.cpp  \
                         $(gstfacetracker_ipp_SROURCES)

if USE_IPP
gstfacetracker_ipp_SROURCES = gstfacetracker/ippimage.cpp
endif



###############################################################################
OBJECTTRACKER_SOURCES = objecttracker/PredatorSrc/cvblobs/ComponentLabeling.cpp 	\
			objecttracker/PredatorSrc/cvblobs/BlobResult.cpp 		\
			objecttracker/PredatorSrc/cvblobs/BlobProperties.cpp 		\
			objecttracker/PredatorSrc/cvblobs/BlobOperators.cpp		\
			objecttracker/PredatorSrc/cvblobs/blob.cpp 			\
			objecttracker/PredatorSrc/cvblobs/BlobContour.cpp		\
			objecttracker/PredatorSrc/tld/VarianceFilter.cpp		\
			objecttracker/PredatorSrc/tld/TLDUtil.cpp			\
			objecttracker/PredatorSrc/tld/TLD.cpp				\
			objecttracker/PredatorSrc/tld/NNClassifier.cpp		\
			objecttracker/PredatorSrc/tld/EnsembleClassifier.cpp		\
			objecttracker/PredatorSrc/tld/DetectorCascade.cpp		\
			objecttracker/PredatorSrc/tld/DetectionResult.cpp		\
			objecttracker/PredatorSrc/tld/Clustering.cpp			\
			objecttracker/PredatorSrc/tld/ForegroundDetector.cpp		\
			objecttracker/PredatorSrc/tld/MedianFlowTracker.cpp		\
			objecttracker/PredatorSrc/mftracker/median.cpp		\
			objecttracker/PredatorSrc/mftracker/lk.cpp			\
			objecttracker/PredatorSrc/mftracker/fbtrack.cpp		\
			objecttracker/PredatorSrc/mftracker/bb.cpp			\
			objecttracker/PredatorSrc/mftracker/bb_predict.cpp		\
			objecttracker/PredatorSrc/wrapper/settings.cpp		\
			objecttracker/PredatorSrc/wrapper/config.cpp			\
			objecttracker/PredatorSrc/wrapper/wrapper.cpp			\
			objecttracker/PredatorSrc/wrapper/predator.cpp		\
			objecttracker/gstobjecttracker.cpp



###############################################################################
FACETRACKING_SOURCES =     facetracking/haarwrapper/haarwrapper.c      \
                           facetracking/facetracking_kernel.c          \
                           facetracking/gstfacetracker3.c              

###############################################################################
BLFACEDETECTOR_SOURCES =   bl_facedetector/gstfacedetector.c         \
                           bl_facedetector/FaceDetect.cpp            \
                           bl_facedetector/history.cpp               \
                           bl_facedetector/SmoothingFilter.cpp       

###############################################################################
IMGSRC_SOURCES =  imgsrc/imgsrc.c     \
                  imgsrc/gstimgsrc.c 

###############################################################################
INTEL_IPP_H264ENC_SOURCES = ipp_h264/gsth264enc.c

#INTEL_IPP_H264ENC_CFLAGS  = $(GST_BASE_CFLAGS)                       \
#                           $(IPP_SAMPLES_CFLAGS)                     \
#                            $(IPP_CFLAGS) 			

#INTEL_IPP_H264ENC_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) -lgstvideo-0.10 -lGLEW

###############################################################################
#IMCOMSG_CFLAGS           = -I$(CURDIR)/../../imcomsg/src/helpers        \
#                           -I$(CURDIR)/../../imcomsg/src                \
#                           -I/usr/include/imcomsg                       
#IMCOMSG_LDFLAGS          = -limcomsg -L$(CURDIR)/../../imcomsg/src/.libs  


###############################################################################
###############################################################################
###############################################################################
###############################################################################
# plugindir is set in configure
plugin_LTLIBRARIES = libgsttsunami.la

# sources used to compile this plug-in
libgsttsunami_la_SOURCES = gsttsunami.c                      \
                       $(TS_OCV_SOURCES)                     \
                       $(FACETRACKING_SOURCES)               \
                       $(BLFACEDETECTOR_SOURCES)             \
                       $(INTEL_IPP_H264ENC_SOURCES)          \
                       ghostmapper/gstghostmapper.c          \
                       blockanalysis/gstblockanalysis.c      \
                       $(IMGSRC_SOURCES)                     \
                       $(GSTFACETRACKER_SOURCES)             \
                       pixelate/gstpixelate.c                \
                       templatematch/gsttemplatematch.c		 \
                       $(OBJECTTRACKER_SOURCES)



# location and name of the imco-libexp library
EXP_REPO_DIR=${CURDIR}/src/.libs

################################################################################
# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgsttsunami_la_CFLAGS = $(GST_CFLAGS)                     \
                          $(TS_OCV_CFLAGS)                  \
                          $(INTEL_IPP_H264ENC_CFLAGS)       \
                          $(IMCOMSG_CFLAGS)                 \
                          $(OPENCV_CFLAGS)                  \
                          $(IPP_SAMPLES_CFLAGS)             \
                          $(IPP_CFLAGS)                     \
                          $(LIBPNG_CFLAGS)                  \
                          $(LIBCURL_CFLAGS)                 \
                          -Wall -Wextra

libgsttsunami_la_CXXFLAGS = $(GST_CFLAGS)                     \
                            $(TS_OCV_CFLAGS)                  \
                            $(INTEL_IPP_H264ENC_CFLAGS)       \
                            $(OPENCV_CFLAGS)                  \
                            $(IPP_SAMPLES_CFLAGS)             \
                            $(IPP_CFLAGS)                     \
                            $(LIBPNG_CFLAGS)                  \
                            $(LIBCURL_CFLAGS)                 \
                            -Wall -Wextra



libgsttsunami_la_LIBADD = $(GST_LIBS)                     \
                         $(TS_OCV_LIBS)                   \
                         $(IPP_LIBS)                      \
                         $(IPP_SAMPLES_LIBS)              \
                         $(LIBPNG_LIBS)                   \
                         $(LIBCURL_LIBS)                  \
                         $(OPENCV_LIBS)

libgsttsunami_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)         \
                           $(TS_OCV_LDFLAGS)             \
                           $(IMCOMSG_LDFLAGS) 

libgsttsunami_la_LIBTOOLFLAGS = --tag=disable-static



#include_HEADERS 
# headers we need but don't want installed
#noinst_HEADERS = 

nodist_libgsttsunami_la_SOURCES = $(ORC_NODIST_SOURCES)

####################################################################################################
####################################################################################################
pkginclude_HEADERS = 

####################################################################################################
# GENERATE GOOGLE BREAKPAD SYMBOLS

SYMSTORE="/usr/bin/symbolstore.py"
DUMPSYM="/usr/bin/dump_syms"

clean_symbols:
	rm -rf $(CURDIR)/../symbols | true

create_symbols: $(plugin_LTLIBRARIES)
	cd $(CURDIR)/.libs && $(SYMSTORE) $(DUMPSYM) $(CURDIR)/../symbols lib*.so

all-local: create_symbols

clean-local: clean_symbols

distclean-local: clean_symbols

.PHONY: create_symbols clean_symbols

#
####################################################################################################
