/*
 * vim:ts=8:sw=3:sts=8:noexpandtab:cino=>5n-3f0^-2{2
 */
#ifdef E_TYPEDEFS

typedef enum _E_Ipc_Domain
{
   E_IPC_DOMAIN_NONE,
   E_IPC_DOMAIN_SETUP,
   E_IPC_DOMAIN_REQUEST,
   E_IPC_DOMAIN_REPLY,
   E_IPC_DOMAIN_EVENT,
   E_IPC_DOMAIN_LAST
} E_Ipc_Domain;

typedef enum _E_Ipc_Op
{
   E_IPC_OP_NONE,
   E_IPC_OP_MODULE_LOAD,
   E_IPC_OP_MODULE_UNLOAD,
   E_IPC_OP_MODULE_ENABLE,
   E_IPC_OP_MODULE_DISABLE,
   E_IPC_OP_MODULE_LIST,
   E_IPC_OP_MODULE_LIST_REPLY,
   E_IPC_OP_MODULE_DIRS_LIST,
   E_IPC_OP_MODULE_DIRS_LIST_REPLY,
   E_IPC_OP_BG_SET,
   E_IPC_OP_BG_GET,
   E_IPC_OP_BG_GET_REPLY,
   E_IPC_OP_BG_DIRS_LIST,
   E_IPC_OP_BG_DIRS_LIST_REPLY,
   E_IPC_OP_FONT_AVAILABLE_LIST,
   E_IPC_OP_FONT_AVAILABLE_LIST_REPLY,
   E_IPC_OP_FONT_APPLY,
   E_IPC_OP_FONT_FALLBACK_CLEAR,
   E_IPC_OP_FONT_FALLBACK_APPEND,
   E_IPC_OP_FONT_FALLBACK_PREPEND,
   E_IPC_OP_FONT_FALLBACK_LIST,
   E_IPC_OP_FONT_FALLBACK_LIST_REPLY,
   E_IPC_OP_FONT_FALLBACK_REMOVE,
   E_IPC_OP_FONT_DEFAULT_SET,
   E_IPC_OP_FONT_DEFAULT_GET,
   E_IPC_OP_FONT_DEFAULT_GET_REPLY,
   E_IPC_OP_FONT_DEFAULT_REMOVE,
   E_IPC_OP_FONT_DEFAULT_LIST,
   E_IPC_OP_FONT_DEFAULT_LIST_REPLY,
   E_IPC_OP_RESTART,
   E_IPC_OP_SHUTDOWN,
   E_IPC_OP_LANG_LIST,
   E_IPC_OP_LANG_LIST_REPLY,
   E_IPC_OP_LANG_SET,
   E_IPC_OP_LANG_GET,
   E_IPC_OP_LANG_GET_REPLY,
   E_IPC_OP_BINDING_MOUSE_LIST,
   E_IPC_OP_BINDING_MOUSE_LIST_REPLY,
   E_IPC_OP_BINDING_MOUSE_ADD,
   E_IPC_OP_BINDING_MOUSE_DEL,
   E_IPC_OP_BINDING_KEY_LIST,
   E_IPC_OP_BINDING_KEY_LIST_REPLY,
   E_IPC_OP_BINDING_KEY_ADD,
   E_IPC_OP_BINDING_KEY_DEL,
   E_IPC_OP_LAST
} E_Ipc_Op;

#else
#ifndef E_IPC_H
#define E_IPC_H

EAPI int  e_ipc_init(void);
EAPI void e_ipc_shutdown(void);

#endif
#endif
