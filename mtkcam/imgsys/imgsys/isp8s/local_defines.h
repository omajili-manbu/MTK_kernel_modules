#ifndef LOCAL_DEFINES_H
#define LOCAL_DEFINES_H

#if __has_include("dt-bindings/memory/mt6993-larb-port.h")
#define IMGSYS_TF_DUMP_8S_L
#endif

#if __has_include("dt-bindings/memory/mt6881-larb-port.h")
#define IMGSYS_TF_DUMP_8S_C
#endif

#endif  /* LOCAL_DEFINES_H */
