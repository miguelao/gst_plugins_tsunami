
AC_DEFUN([YUV_CHECK],
[
    AC_ARG_ENABLE(yuv,
  AS_HELP_STRING([--enable-yuv],[use YUV3 instead of RGBA]),
  [case "${enableval}" in
    auto) enable_yuv=auto ;;
    yes) enable_yuv=yes ;;
    no)  enable_yuv=no ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-yuv) ;;
  esac
  ],
  [enable_yuv=yes]) dnl Default value

  if test "x$enable_yuv" != "xno" ; then
      AC_DEFINE(FACETRK_FORMAT, FACETRK_FORMAT_YUV, [Colourspace for face tracking])
      YUV=yes
      YUVA=no
  else
      AC_DEFINE(FACETRK_FORMAT, FACETRK_FORMAT_RGBA, [Colourspace for face tracking])
      YUV=no
      YUVA=no
  fi
  AM_CONDITIONAL(USE_YUV, [test "x$YUV" = "xyes"])

])

AC_DEFUN([YUV_OUTPUT],
[
  if test "$YUV" = yes ; then
    printf "configure: *** Colourspace for cutout and face tracking is YUV3\n"
  elif test "$YUVA" = yes ; then
    printf "configure: *** Colourspace for cutout and face tracking is YUVA\n"
  else
    printf "configure: *** Colourspace for cutout and face tracking is RGBA\n"
  fi
])

