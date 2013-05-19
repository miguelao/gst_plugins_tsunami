CXX = g++
C = gcc
CFLAGS = `pkg-config --cflags --libs gstreamer-0.10` -fPIC -c -I/usr/include/opencv -ggdb3
CXXFLAGS = -fPIC -I/usr/include/opencv/ -I/usr/include/opencv/ -I/usr/include/libsvm-2.0/libsvm/ -ggdb3
OBJFLAGS = -shared -Wl,-soname,libgstfacedetectorlib.so -o libgstfacedetectorlib.so -lm -lcv -lhighgui
SRC = SmoothingFilter.cpp FaceDetect.cpp history.cpp 
CSRC = gstfacedetector.c
OBJ = $(addsuffix .o, $(basename $(SRC))) 
COBJ = $(addsuffix .o, $(basename $(CSRC)))

output: $(OBJ) 
	$(C) $(OBJ) $(OBJFLAGS)
	#sudo ln -sf $(PWD)/libgstobjectdetectorV2.so /usr/lib/gstreamer-0.10/libgstobjectdetectorV2.so
	#cp libgstfacedetectorlib.so ../FPackage/

$(OBJ): $(SRC) common.h history.h FaceDetect.h FaceDetectLib.h SmoothingFilter.h
	$(CXX) $(CXXFLAGS) -c $(addsuffix .cpp, $(basename $@)) -o $@

$(COBJ): $(CSRC) gstfacedetector.h
	$(C) $(CFLAGS) -c $(CSRC) -o $@

clean:
	rm -rf *.o *.so
