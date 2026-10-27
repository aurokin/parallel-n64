#ifndef _3DMATH_H
#define _3DMATH_H

#ifdef __cplusplus
extern "C" {
#endif

enum gfx_plugin_type
{
   GFX_PARALLEL = 4,
};

enum rsp_plugin_type
{
   RSP_HLE = 0,
   RSP_CXD4,
   RSP_PARALLEL
};

extern enum gfx_plugin_type gfx_plugin;

#ifdef __cplusplus
}
#endif

#endif
