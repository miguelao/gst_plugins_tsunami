dnl -*- autoconf -*-
dnl specific:

AC_DEFUN([IPP_CHECK],
[

REQ=ifelse([$1], , "optional", "required")

AC_ARG_WITH( ipp,
    [  --with-ipp              Set prefix where Intel IPP can be found (ex:/usr or /usr/local) [default=/opt/intel/ipp] ],
    [ ipp_prefix=${withval}],
    [ ipp_prefix=/opt/intel/ipp ])

    if test "x$ipp_prefix" == xno ; then

        if test "x$REQ" == xrequired ; then
          AC_MSG_ERROR([Project needs IPP and --witout-ipp specified])
        fi

        AC_DEFINE(DISABLE_IPP, 1, [Disable IPP])
        USE_IPP=no
        IPP_CFLAGS=""
        IPP_LIBS=""
    else
        if test "$ipp_prefix" != "/usr" ; then
            IPP_CFLAGS="-DLINUX32 -DLINUX64 -I${ipp_prefix}/include"
        fi
        #check IPP SAMPLES headers
        CPPFLAGS_save=$CPPFLAGS
        CPPFLAGS=$IPP_CFLAGS
        AC_LANG([C++])
        AC_CHECK_HEADER([ipp.h], ,AC_MSG_ERROR([Could not find IPP headers - install mosami-ipp]))
        CPPFLAGS=$CPPFLAGS_save
    
        dnl check for IPP libs
        if test "$ipp_prefix" != "/usr" ; then
            IPP_LIBS="-L$ipp_prefix/lib/intel64" 
        fi
        
        IPP_LIBS="$IPP_LIBS -lippdc -lippcc -lippac -lippvc -lippcv -lippj -lippi -lipps -lippsc -lippcore -ldl  -lpthread"
        LDFLAGS_save=$LDFLAGS
        LDFLAGS=$IPP_LIBS
        LIBS_save=$LIBS
        
        AC_LANG([C++])
        
        #Does not work for oneiric?
        #AC_CHECK_LIB(ippcore,ippGetLibVersion, , AC_MSG_ERROR([Could not find IPP ippcore library - install mosami-ipp]),[-lippcore  ])
        #AC_CHECK_LIB(ippcore,ippStaticInit, , AC_MSG_ERROR([Could not find IPP ippcore library - install mosami-ipp]),[-lippcore  ])
        
        AC_DEFINE(USE_IPP, 1, [Use IPP])
        USE_IPP=yes
        
        LDFLAGS=$LDFLAGS_save
        LIBS=$LIBS_save
    fi

    AM_CONDITIONAL(USE_IPP, [test "x$USE_IPP" = "xyes"])
    AC_SUBST(IPP_CFLAGS)
    AC_SUBST(IPP_LIBS)
])


AC_DEFUN([IPP_SAMPLES_CHECK],
[
AC_ARG_WITH( ipp-samples,
    [  --with-ipp-samples      Set prefix where Intel IPP samples can be found (ex:/usr or /usr/local) [default=/opt/intel/ipp-samples] ],
    [ ipp_samples_prefix=${withval}],
    [ ipp_samples_prefix=/opt/intel/ipp-samples ])
		
    if test "x$ipp_samples_prefix" == xno ; then
        if test "x$REQ" == xrequired ; then
          AC_MSG_ERROR([Project needs IPP and --witout-ipp specified])
        fi
        IPP_SAMPLES_CFLAGS=""
        IPP_SAMPLES_LIBS=""
    else

        if test "$ipp_samples_prefix" != "/usr" ; then
            IPP_SAMPLES_CFLAGS="-DLINUX32 -DLINUX64 -I${ipp_samples_prefix}/include -I/opt/intel/ipp/include"
        fi

        dnl check IPP SAMPLES headers
        	CPPFLAGS_save=$CPPFLAGS
        	CPPFLAGS=$IPP_SAMPLES_CFLAGS
        	AC_LANG([C++])
        	AC_CHECK_HEADER([umc.h], ,AC_MSG_ERROR([Could not find IPP SAMPLES headers - install mosami-ipp-samples-dev]))
        	CPPFLAGS=$CPPFLAGS_save
        
        dnl check for IPP SAMPLES libs
        	if test "$ipp_samples_prefix" != "/usr" ; then
        		IPP_SAMPLES_LIBS="-L$ipp_samples_prefix/lib" 
        	fi
        	
        	IPP_SAMPLES_LIBS="$IPP_SAMPLES_LIBS  -lvc1_enc -lavs_enc -lavs_dec -lavs_common -lscene_analyzer  -lh264_enc -lcolor_space_converter -ldv100_enc \
        -lvc1_common -lme -lumc_io -lmedia_buffers -lcommon -lumc -lvm_plus -lvm $IPP_LIBS"
        	LDFLAGS_save=$LDFLAGS
        	LDFLAGS=$IPP_SAMPLES_LIBS
        	LIBS_save=$LIBS
        	AC_LANG([C++])
        # TODO complete libs check	
        	
        #	AC_CHECK_LIB(h264_enc,[UMC::H264VideoEncoder::H264VideoEncoder], , AC_MSG_ERROR([Could not find IPP h264_enc library !]),[-lh264_enc  ])
        	LDFLAGS=$LDFLAGS_save
        	LIBS=$LIBS_save
    fi

    AC_SUBST(IPP_SAMPLES_CFLAGS)
    AC_SUBST(IPP_SAMPLES_LIBS)

])

AC_DEFUN([IPP_OUTPUT],
[
  if test "$USE_IPP" = yes ; then
    printf "configure: *** Intel IPP acceleration enabled (${ipp_prefix})\n"
  else
    printf "configure: *** IPP acceleration disabled.\n"
  fi

])

AC_DEFUN([IPP_SAMPLES_OUTPUT],
[
	printf "configure: *** Intel IPP samples at ${ipp_samples_prefix}\n"
])
