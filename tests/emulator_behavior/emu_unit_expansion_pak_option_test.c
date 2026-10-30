/* The legacy config bridge already wires this option into main_init(). */
#define M64P_CORE_PROTOTYPES 1
#include "api/config.h"
#include "api/m64p_config.h"
#include <libretro.h>
#undef NDEBUG
#include <assert.h>
#include <string.h>

unsigned char rsp_conf[32];
const char* retro_get_system_directory(void) { return "."; }
void DebugMessage(int level, const char* fmt, ...) { (void)level; (void)fmt; }
static const char* selected;
static bool environment(unsigned command, void* data)
{
    assert(command == RETRO_ENVIRONMENT_GET_VARIABLE);
    struct retro_variable* var = data;
    assert(strcmp(var->key, "parallel-n64-disable_expmem") == 0);
    var->value = selected;
    return selected != NULL;
}
retro_environment_t environ_cb = environment;

int main(void)
{
    m64p_handle core;
    assert(ConfigInit(NULL, NULL) == M64ERR_SUCCESS);
    assert(ConfigOpenSection("Core", &core) == M64ERR_SUCCESS);
    assert(ConfigSetDefaultBool(core, "DisableExtraMem", 0, NULL) == M64ERR_SUCCESS);
    selected = NULL;
    assert(ConfigGetParamInt(core, "DisableExtraMem") == 0); /* Default: 8 MiB. */
    selected = "enabled";
    assert(ConfigGetParamInt(core, "DisableExtraMem") == 0);
    selected = "disabled";
    assert(ConfigGetParamInt(core, "DisableExtraMem") == 1); /* main_init: 4 MiB. */
    selected = "enabled";
    assert(ConfigGetParamInt(core, "DisableExtraMem") == 0);
    assert(ConfigShutdown() == M64ERR_SUCCESS);
    return 0;
}
