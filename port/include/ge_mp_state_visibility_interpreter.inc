/* The original parser's function-static object retains exactly one lifetime
 * per interpreter. Name it at file scope for read-only typed state export. */
#ifdef GE_RENDERER_BG_VISIBILITY_NAMESPACE_H
#define ge_bg_visibility_interpreter_state ge_renderer_private_interpreter_state
#endif
Unk80081600 ge_bg_visibility_interpreter_state;
#define dword_CODE_bss_80081600 ge_bg_visibility_interpreter_state
