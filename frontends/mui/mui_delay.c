#include "mui/mui.h"
#include "mui/schedule.h"
#include "utils/log0.h"

#include <proto/exec.h>

struct mui_delayed_method_ctx {
    Object *app;
    Object *obj;
    ULONG method_id;
    ULONG numargs;
};

static void mui_delayed_method_invoke(void *p)
{
    struct mui_delayed_method_ctx *ctx = (struct mui_delayed_method_ctx *)p;

    if (ctx == NULL) {
        return;
    }

    LOG(("DEBUG: Executing delayed method %lu on %p", ctx->method_id, ctx->obj));

    DoMethod(ctx->app,
             MUIM_Application_PushMethod,
             ctx->obj,
             ctx->numargs,
             ctx->method_id);

    FreeMem(ctx, sizeof(*ctx));
}

void mui_queue_method_delay(Object *app, Object *obj, ULONG delay_ms, ULONG method_id)
{
    if (app == NULL || obj == NULL) {
        LOG(("ERROR: mui_queue_method_delay called with NULL app/obj"));
        return;
    }

#ifdef MUIV_PushMethod_Delay
    if (mui_supports_pushmethod_delay) {
        DoMethod(app,
                 MUIM_Application_PushMethod,
                 obj,
                 1 | MUIV_PushMethod_Delay(delay_ms),
                 method_id);
        return;
    }
#endif

    if (delay_ms == 0) {
        DoMethod(app, MUIM_Application_PushMethod, obj, 1, method_id);
        return;
    }

    struct mui_delayed_method_ctx *ctx =
        AllocMem(sizeof(*ctx), MEMF_CLEAR);

    if (ctx == NULL) {
        LOG(("ERROR: Failed to allocate delayed method context, running immediately"));
        DoMethod(app, MUIM_Application_PushMethod, obj, 1, method_id);
        return;
    }

    ctx->app = app;
    ctx->obj = obj;
    ctx->method_id = method_id;
    ctx->numargs = 1;

    nserror err = mui_schedule((int)delay_ms, mui_delayed_method_invoke, ctx);
    if (err != NSERROR_OK) {
        LOG(("ERROR: mui_schedule failed (%d), running method immediately", err));
        FreeMem(ctx, sizeof(*ctx));
        DoMethod(app, MUIM_Application_PushMethod, obj, 1, method_id);
    }
}
