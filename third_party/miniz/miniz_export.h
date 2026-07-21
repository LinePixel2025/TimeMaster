#pragma once

#ifdef MINIZ_STATIC_DEFINE
#  define MINIZ_EXPORT
#else
#  ifdef miniz_EXPORTS
#    define MINIZ_EXPORT __declspec(dllexport)
#  else
#    define MINIZ_EXPORT __declspec(dllimport)
#  endif
#endif
