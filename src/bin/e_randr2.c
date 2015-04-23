#include "e.h"

#define E_RANDR_CONFIG_VERSION 1

/////////////////////////////////////////////////////////////////////////
static Eina_Bool               _screen_closed(E_Randr2_Screen *s);
static void                    _animated_apply_abort(void);
static Eina_Bool               _cb_delay_timer(void *data);
static Eina_Bool               _cb_fade_animator(void *data);
static void                    _animated_apply(void);
static void                    _do_apply(void);
static void                    _info_free(E_Randr2 *r);
static E_Config_Randr2        *_config_load(void);
static void                    _config_free(E_Config_Randr2 *cfg);
static Eina_Bool               _config_save(E_Randr2 *r, E_Config_Randr2 *cfg);
static void                    _config_update(E_Randr2 *r, E_Config_Randr2 *cfg);
static void                    _config_apply(E_Randr2 *r, E_Config_Randr2 *cfg);
static int                     _config_screen_match_count(E_Randr2 *r, E_Config_Randr2 *cfg);
static char                   *_screens_fingerprint(E_Randr2 *r);
static Eina_Bool               _screens_differ(E_Randr2 *r1, E_Randr2 *r2);
static Eina_Bool               _cb_screen_change_delay(void *data);
static E_Randr2_Screen        *_screen_output_find(const char *out);
static E_Randr2_Screen        *_screen_id_find(const char *id);
static void                    _screen_config_takeover(void);
static void                    _screen_config_do(E_Randr2_Screen *s);
static void                    _screen_config_eval(void);
static void                    _screen_config_maxsize(void);

/////////////////////////////////////////////////////////////////////////
static E_Config_DD   *_e_randr2_cfg_edd = NULL;
static E_Config_DD   *_e_randr2_cfg_screen_edd = NULL;
static Eina_List     *_ev_handlers = NULL;
static Ecore_Timer   *_screen_delay_timer = NULL;
static Eina_Bool      event_screen = EINA_FALSE;
static Eina_Bool      event_ignore = EINA_FALSE;

/////////////////////////////////////////////////////////////////////////
EAPI E_Config_Randr2 *e_randr2_cfg = NULL;
EAPI E_Randr2        *e_randr2 = NULL;

EAPI int              E_EVENT_RANDR_CHANGE = 0;

/////////////////////////////////////////////////////////////////////////
EINTERN Eina_Bool
e_randr2_init(void)
{
   if (!E_EVENT_RANDR_CHANGE) E_EVENT_RANDR_CHANGE = ecore_event_type_new();
   if ((!e_comp->screen) || (!e_comp->screen->available) || (!e_comp->screen->available())) return EINA_FALSE;
   // create data descriptors for config storage
   _e_randr2_cfg_screen_edd =
     E_CONFIG_DD_NEW("E_Config_Randr2_Screen", E_Config_Randr2_Screen);
#undef T
#undef D
#define T E_Config_Randr2_Screen
#define D _e_randr2_cfg_screen_edd
   E_CONFIG_VAL(D, T, id, STR);
   E_CONFIG_VAL(D, T, rel_to, STR);
   E_CONFIG_VAL(D, T, rel_align, DOUBLE);
   E_CONFIG_VAL(D, T, mode_refresh, DOUBLE);
   E_CONFIG_VAL(D, T, mode_w, UINT);
   E_CONFIG_VAL(D, T, mode_h, INT);
   E_CONFIG_VAL(D, T, rotation, INT);
   E_CONFIG_VAL(D, T, priority, INT);
   E_CONFIG_VAL(D, T, rel_mode, UCHAR);
   E_CONFIG_VAL(D, T, enabled, UCHAR);

   _e_randr2_cfg_edd = E_CONFIG_DD_NEW("E_Config_Randr2", E_Config_Randr2);
#undef T
#undef D
#define T E_Config_Randr2
#define D _e_randr2_cfg_edd
   E_CONFIG_VAL(D, T, version, INT);
   E_CONFIG_LIST(D, T, screens, _e_randr2_cfg_screen_edd);
   E_CONFIG_VAL(D, T, restore, UCHAR);
   E_CONFIG_VAL(D, T, ignore_hotplug_events, UCHAR);
   E_CONFIG_VAL(D, T, ignore_acpi_events, UCHAR);

   // set up events from the driver
   if (e_comp->screen->init)
     e_comp->screen->init();
   // get current screen info
   e_randr2 = e_comp->screen->create();
   // from screen info calculate screen max dimensions
   _screen_config_maxsize();
   // load config and apply it
   e_randr2_cfg = _config_load();
   // only apply if restore is set AND at least one configured screen
   // matches one we have
   if ((e_randr2_cfg->restore) &&
       (_config_screen_match_count(e_randr2, e_randr2_cfg) > 0))
     {
        _do_apply();
     }
   else
     {
        _config_update(e_randr2, e_randr2_cfg);
        e_randr2_config_save();
     }
   return EINA_TRUE;
}

EINTERN int
e_randr2_shutdown(void)
{
   _animated_apply_abort();
   // nuke any screen config delay handler
   if (_screen_delay_timer) ecore_timer_del(_screen_delay_timer);
   _screen_delay_timer = NULL;
   // stop listening to driver info
   if (e_comp->screen->shutdown)
     e_comp->screen->shutdown();
   // clear up all event handlers
   E_FREE_LIST(_ev_handlers, ecore_event_handler_del);
   // free up screen info
   _info_free(e_randr2);
   e_randr2 = NULL;
   _config_free(e_randr2_cfg);
   e_randr2_cfg = NULL;
   // free up data descriptors
   E_CONFIG_DD_FREE(_e_randr2_cfg_edd);
   E_CONFIG_DD_FREE(_e_randr2_cfg_screen_edd)
   return 1;
}

EAPI Eina_Bool
e_randr2_config_save(void)
{
   // save our config
   return _config_save(e_randr2, e_randr2_cfg);
}

EAPI void
e_randr2_config_apply(void)
{
   _animated_apply();
}

EAPI void
e_randr2_screeninfo_update(void)
{
   // re-fetch/update current screen info
   _info_free(e_randr2);
   e_randr2 = e_comp->screen->create();
   _screen_config_maxsize();
}

/////////////////////////////////////////////////////////////////////////
static double _start_time = 0.0;
static Ecore_Animator *_fade_animator = NULL;
static Ecore_Timer *_apply_delay = NULL;
Eina_Bool _applying = EINA_FALSE;
static int _target_from = 0;
static int _target_to = 0;
static Evas_Object *_fade_obj = NULL;

static Eina_Bool
_screen_closed(E_Randr2_Screen *s)
{
   printf("RRR: check lid for %s...\n", s->info.name);
   if (!e_acpi_lid_is_closed()) return EINA_FALSE;
   if (s->info.is_lid)
     {
        printf("RRR:   is closed lid\n");
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

static void
_animated_apply_abort(void)
{
   if (_apply_delay) ecore_timer_del(_apply_delay);
   if (_fade_animator) ecore_animator_del(_fade_animator);
   _apply_delay = NULL;
   _fade_animator = NULL;
   _applying = EINA_FALSE;
   _fade_obj = NULL;
}

static Eina_Bool
_cb_delay_timer(void *data EINA_UNUSED)
{
   _apply_delay = NULL;
   _target_from = 255;
   _target_to = 0;
   _start_time = ecore_loop_time_get();
   _fade_animator = ecore_animator_add(_cb_fade_animator, NULL);
   return EINA_FALSE;
}

static Eina_Bool
_cb_fade_animator(void *data EINA_UNUSED)
{
   double t = ecore_loop_time_get() - _start_time;
   int v;

   t = t / 0.5;
   if (t < 0.0) t = 0.0;
   v = _target_from + ((_target_to - _target_from) * t);
   if (t >= 1.0) v = _target_to;
   evas_object_color_set(_fade_obj, 0, 0, 0, v);
   if (v == _target_to)
     {
        if (_target_to == 255)
          {
             _apply_delay = ecore_timer_add(3.0, _cb_delay_timer, NULL);
             _do_apply();
          }
        else
          {
             evas_object_del(_fade_obj);
             _fade_obj = NULL;
             _applying = EINA_FALSE;
          }
        _fade_animator = NULL;
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

static void
_animated_apply(void)
{
   Evas *e;

   // fade out, config, wait 3 seconds, fade back in
   if (_applying) return;
   _applying = EINA_TRUE;
   _start_time = ecore_loop_time_get();
   e = e_comp->evas;
   _fade_obj = evas_object_rectangle_add(e);
   evas_object_pass_events_set(_fade_obj, EINA_TRUE);
   evas_object_color_set(_fade_obj, 0, 0, 0, 0);
   evas_object_move(_fade_obj, 0, 0);
   evas_object_resize(_fade_obj, 999999, 999999);
   evas_object_layer_set(_fade_obj, EVAS_LAYER_MAX);
   evas_object_show(_fade_obj);
   _target_from = 0;
   _target_to = 255;
   _fade_animator = ecore_animator_add(_cb_fade_animator, NULL);
}

static void
_do_apply(void)
{
   // take current screen config and apply it to the driver
   printf("RRR: re-get info before applying..\n");
   _info_free(e_randr2);
   e_randr2 = e_comp->screen->create();
   _screen_config_maxsize();
   printf("RRR: apply config...\n");
   _config_apply(e_randr2, e_randr2_cfg);
   printf("RRR: takeover config...\n");
   _screen_config_takeover();
   printf("RRR: eval config...\n");
   _screen_config_eval();
   printf("RRR: really apply config...\n");
   e_comp->screen->apply();
   printf("RRR: done config...\n");
}

static void
_info_free(E_Randr2 *r)
{
   E_Randr2_Screen *s;
   E_Randr2_Mode *m;

   if (!r) return;
   // free up our randr screen data
   EINA_LIST_FREE(r->screens, s)
     {
        free(s->id);
        free(s->info.screen);
        free(s->info.name);
        free(s->info.edid);
        EINA_LIST_FREE(s->info.modes, m) free(m);
        free(s->config.relative.to);
        free(s);
     }
   free(r);
}

static E_Config_Randr2 *
_config_load(void)
{
   E_Config_Randr2 *cfg;

   // load config and check if version is up to date
   cfg = e_config_domain_load("e_randr2", _e_randr2_cfg_edd);
   if (cfg)
     {
        if (cfg->version < E_RANDR_CONFIG_VERSION)
          {
             _config_free(cfg);
             cfg = NULL;
          }
        else
          {
             printf("RRR: loaded existing config\n");
             return cfg;
          }
     }

   // need new config
   cfg = calloc(1, sizeof(E_Config_Randr2));
   cfg->version = E_RANDR_CONFIG_VERSION;
   cfg->screens = NULL;
   cfg->restore = 1;
   cfg->ignore_hotplug_events = 0;
   cfg->ignore_acpi_events = 0;
   printf("RRR: fresh config\n");
   return cfg;
}

static void
_config_free(E_Config_Randr2 *cfg)
{
   E_Config_Randr2_Screen *cs;

   if (!cfg) return;
   // free config data
   EINA_LIST_FREE(cfg->screens, cs) free(cs);
   free(cfg);
}

static Eina_Bool
_config_save(E_Randr2 *r, E_Config_Randr2 *cfg)
{
   if ((!r) || (!cfg)) return EINA_FALSE;
   // save config struct to cfg file
   return e_config_domain_save("e_randr2", _e_randr2_cfg_edd, cfg);
}

static void
_config_update(E_Randr2 *r, E_Config_Randr2 *cfg)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   E_Config_Randr2_Screen *cs;

   printf("--------------------------------------------------\n");
   EINA_LIST_FOREACH(r->screens, l, s)
     {
        printf("RRR: out id=%s:  connected=%i\n", s->id, s->info.connected);
        if ((!s->id) || (!s->info.connected) || (_screen_closed(s))) continue;
        cs = e_randr2_config_screen_find(s, cfg);
        if (!cs)
          {
             cs = calloc(1, sizeof(E_Config_Randr2_Screen));
             if (cs)
               {
                  cs->id = strdup(s->id);
                  cfg->screens = eina_list_prepend(cfg->screens, cs);
               }
          }
        if (cs)
          {
             if (s->config.relative.to)
               cs->rel_to = strdup(s->config.relative.to);
             cs->rel_align = s->config.relative.align;
             cs->mode_refresh = s->config.mode.refresh;
             cs->mode_w = s->config.mode.w;
             cs->mode_h = s->config.mode.h;
             cs->rotation = s->config.rotation;
             cs->priority = s->config.priority;
             cs->rel_mode = s->config.relative.mode;
             cs->enabled = s->config.enabled;
          }
     }
   printf("--------------------------------------------------\n");
}

static void
_config_apply(E_Randr2 *r, E_Config_Randr2 *cfg)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   E_Config_Randr2_Screen *cs;

   if ((!r) || (!cfg)) return;
   EINA_LIST_FOREACH(r->screens, l, s)
     {
        printf("RRR: apply '%s'...\n", s->info.name);
        cs = NULL;
        if ((!_screen_closed(s)) && (s->info.connected))
          cs = e_randr2_config_screen_find(s, cfg);
        printf("RRR: connected =  %i\n", s->info.connected);
        if ((cs) && (cs->enabled))
          {
             printf("RRR: ... enabled\n");
             s->config.enabled = EINA_TRUE;
             s->config.mode.w = cs->mode_w;
             s->config.mode.h = cs->mode_h;
             s->config.mode.refresh = cs->mode_refresh;
             s->config.mode.preferred = EINA_FALSE;
             s->config.rotation = cs->rotation;
             s->config.priority = cs->priority;
             printf("RRR: ... priority = %i\n", cs->priority);
             free(s->config.relative.to);
             if (cs->rel_to) s->config.relative.to = strdup(cs->rel_to);
             else s->config.relative.to = NULL;
             s->config.relative.mode = cs->rel_mode;
             s->config.relative.align = cs->rel_align;
          }
        else
          {
             printf("RRR: ... disabled\n");
             s->config.enabled = EINA_FALSE;
             s->config.geom.x = 0;
             s->config.geom.y = 0;
             s->config.geom.w = 0;
             s->config.geom.h = 0;
             s->config.mode.w = 0;
             s->config.mode.h = 0;
             s->config.mode.refresh = 0.0;
             s->config.mode.preferred = EINA_FALSE;
             s->config.rotation = 0;
             s->config.priority = 0;
             free(s->config.relative.to);
             s->config.relative.to = NULL;
             s->config.relative.mode = E_RANDR2_RELATIVE_NONE;
             s->config.relative.align = 0.0;
          }
        s->config.configured = EINA_TRUE;
     }
}

static int
_config_screen_match_count(E_Randr2 *r, E_Config_Randr2 *cfg)
{
   Eina_List *l, *ll;
   E_Randr2_Screen *s;
   E_Config_Randr2_Screen *cs;
   int count = 0;

   EINA_LIST_FOREACH(cfg->screens, l, cs)
     {
        if (!cs->id) continue;
        EINA_LIST_FOREACH(r->screens, ll, s)
          {
             if ((!s->id) || (!s->info.connected) ||
                 (_screen_closed(s))) continue;
             if (!strcmp(cs->id, s->id)) count++;
          }
     }
   return count;
}

static char *
_screens_fingerprint(E_Randr2 *r)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   Eina_Strbuf *buf;
   char *str;

   buf = eina_strbuf_new();
   if (!buf) return NULL;
   EINA_LIST_FOREACH(r->screens, l, s)
     {
        if (!s->id) eina_strbuf_append(buf, ":NULL:");
        else
          {
             eina_strbuf_append(buf, ":");
             eina_strbuf_append(buf, s->id);
             eina_strbuf_append(buf, ":");
             if (s->info.lid_closed) eina_strbuf_append(buf, ":LC:");
             else eina_strbuf_append(buf, ":LO:");
          }
     }
   str = eina_strbuf_string_steal(buf);
   eina_strbuf_free(buf);
   return str;
}

static Eina_Bool
_screens_differ(E_Randr2 *r1, E_Randr2 *r2)
{
   char *s1, *s2;
   Eina_Bool changed = EINA_FALSE;
   Eina_List *l, *ll;
   E_Randr2_Screen *s, *ss;

   // check monitor outputs and edids, plugged in things
   s1 = _screens_fingerprint(r1);
   s2 = _screens_fingerprint(r2);
   if ((!s1) && (!s2)) return EINA_FALSE;
   printf("RRR: check fingerprint...\n");
   if ((s1) && (s2) && (strcmp(s1, s2))) changed = EINA_TRUE;
   printf("RRR: ... fingerprint says %i\n", changed);
   free(s1);
   free(s2);
   // check screen config
   EINA_LIST_FOREACH(r2->screens, l, s)
     {
        if (!s->id) continue;
        EINA_LIST_FOREACH(r2->screens, ll, ss)
          {
             if ((ss->id) && (!strcmp(s->id, ss->id))) break;
             ss = NULL;
          }
        if (!ss) changed = EINA_TRUE;
        else if ((s->config.geom.x != ss->config.geom.x) ||
                 (s->config.geom.y != ss->config.geom.y) ||
                 (s->config.geom.w != ss->config.geom.w) ||
                 (s->config.geom.h != ss->config.geom.h) ||
                 (s->config.mode.w != ss->config.mode.w) ||
                 (s->config.mode.h != ss->config.mode.h) ||
                 (s->config.enabled != ss->config.enabled) ||
                 (s->config.rotation != ss->config.rotation) ||
                 (s->info.lid_closed != ss->info.lid_closed))
          changed = EINA_TRUE;
     }
   printf("RRR: changed = %i\n", changed);
   return changed;
}

static Eina_Bool
_cb_screen_change_delay(void *data EINA_UNUSED)
{
   _screen_delay_timer = NULL;
   printf("RRR: ... %i %i\n", event_screen, event_ignore);
   // if we had a screen plug/unplug etc. event and we shouldnt ignore it...
   if ((event_screen) && (!event_ignore))
     {
        E_Randr2 *rtemp;
        Eina_Bool change = EINA_FALSE;

        printf("RRR: reconfigure screens due to event...\n");
        rtemp = e_comp->screen->create();
        if (rtemp)
          {
             if (_screens_differ(e_randr2, rtemp)) change = EINA_TRUE;
             _info_free(rtemp);
          }
        printf("RRR: change = %i\n", change);
        // we plugged or unplugged some monitor - re-apply config so
        // known screens can be coonfigured
        if (change) e_randr2_config_apply();
     }
   // update screen info after the above apply or due to external changes
   e_randr2_screeninfo_update();
   // tell the rest of e some screen reconfigure thing happened
   ecore_event_add(E_EVENT_RANDR_CHANGE, NULL, NULL, NULL);
   event_screen = EINA_FALSE;
   event_ignore = EINA_FALSE;
   return EINA_FALSE;
}

static E_Randr2_Screen *
_screen_output_find(const char *out)
{
   E_Randr2_Screen *s;
   Eina_List *l;
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (!strcmp(s->info.name, out)) return s;
     }
   return NULL;
}

static E_Randr2_Screen *
_screen_id_find(const char *id)
{
   E_Randr2_Screen *s;
   Eina_List *l;
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (!strcmp(s->id, id)) return s;
     }
   return NULL;
}

static void
_screen_config_takeover(void)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        s->config.configured = EINA_TRUE;
     }
}

static E_Config_Randr2_Screen *_config_screen_string_find(E_Config_Randr2 *cfg, const char *id);
static E_Randr2_Screen *_screen_fuzzy_fallback_find(E_Config_Randr2 *cfg, const char *id);

static E_Config_Randr2_Screen *
_config_screen_string_find(E_Config_Randr2 *cfg, const char *id)
{
   Eina_List *l;
   E_Config_Randr2_Screen *cs;

   if ((!id) || (!cfg)) return NULL;
   EINA_LIST_FOREACH(cfg->screens, l, cs)
     {
        if (!cs->id) continue;
        if (!strcmp(cs->id, id)) return cs;
     }
   return NULL;
}

static E_Randr2_Screen *
_screen_fuzzy_fallback_find(E_Config_Randr2 *cfg, const char *id)
{
   E_Randr2_Screen *s = NULL;
   char *p, *name;

   // strip out everythng in the string from / on as that is edid
   // and fall back to finding just the output name in the rel
   // to identifier, rather than the specific screen id
   name = alloca(strlen(id) + 1);
   strcpy(name, id);
   if ((p = strchr(name, '/'))) *p = 0;

   s = _screen_id_find(id);
   if (!s) s = _screen_output_find(name);
   if (!s)
     {
        E_Config_Randr2_Screen *cs;

        cs = _config_screen_string_find(cfg, id);
        if ((cs) && (cs->id)) return _screen_fuzzy_fallback_find(cfg, cs->id);
     }
   return s;
}

static E_Config_Randr2_Screen *
_config_screen_clone_resolve(E_Config_Randr2 *cfg, const char *id, int *x, int *y)
{
   E_Config_Randr2_Screen *cs;
   E_Randr2_Screen *s;
   char *p, *name;

   cs = _config_screen_string_find(cfg, id);
   if (!cs) return NULL;

   name = alloca(strlen(cs->id) + 1);
   strcpy(name, cs->id);
   if ((p = strchr(name, '/'))) *p = 0;

   s = _screen_id_find(cs->id);
   if (!s) s = _screen_output_find(name);
   if (!s)
     {
        if ((cs->rel_mode == E_RANDR2_RELATIVE_CLONE) && (cs->rel_to))
          return _config_screen_clone_resolve(cfg, cs->rel_to, x, y);
        return NULL;
     }
   _screen_config_do(s);
   *x = s->config.geom.x;
   *y = s->config.geom.y;
   return cs;
}

static int _config_do_recurse = 0;

static void
_screen_config_do(E_Randr2_Screen *s)
{
   E_Randr2_Screen *s2 = NULL;

   printf("RRR: screen do '%s'\n", s->info.name);
   if (_config_do_recurse > 10)
     {
        ERR("screen config loop!");
        return;
     }
   _config_do_recurse++;
   // if screen has a dependency...
   if ((s->config.relative.mode != E_RANDR2_RELATIVE_UNKNOWN) &&
       (s->config.relative.mode != E_RANDR2_RELATIVE_NONE) &&
       (s->config.relative.to))
     {
        // if this screen is relative TO something (clone or left/right etc.
        // then calculate what it is relative to first
        s2 = _screen_fuzzy_fallback_find(e_randr2_cfg, s->config.relative.to);
        printf("RRR: '%s' is relative to '%s'\n", s->info.name, s2 ? s2->info.name : "NONE");
        if (s2)
          {
             _screen_config_do(s2);
             if (!s2->config.enabled) s2 = NULL;
          }
     }
   s->config.geom.x = 0;
   s->config.geom.y = 0;
   if ((s->config.rotation == 0) || (s->config.rotation == 180))
     {
        s->config.geom.w = s->config.mode.w;
        s->config.geom.h = s->config.mode.h;
     }
   else
     {
        s->config.geom.w = s->config.mode.h;
        s->config.geom.h = s->config.mode.w;
     }
   if (s2)
     {
        if (s->config.relative.mode == E_RANDR2_RELATIVE_CLONE)
          {
             printf("RRR: clone relative\n");
             s->config.geom.x = s2->config.geom.x;
             s->config.geom.y = s2->config.geom.y;
             s->config.geom.w = s2->config.geom.w;
             s->config.geom.h = s2->config.geom.h;
             s->config.mode.w = s2->config.mode.w;
             s->config.mode.h = s2->config.mode.h;
             s->config.rotation = s2->config.rotation;
             s->config.mode.refresh = s2->config.mode.refresh;
          }
        else if (s->config.relative.mode == E_RANDR2_RELATIVE_TO_LEFT)
          {
             printf("RRR: to left relative\n");
             s->config.geom.x = s2->config.geom.x - s->config.geom.w;
             s->config.geom.y = s2->config.geom.y +
             ((s2->config.geom.h - s->config.geom.h) *
              s->config.relative.align);
          }
        else if (s->config.relative.mode == E_RANDR2_RELATIVE_TO_RIGHT)
          {
             printf("RRR: to right relative\n");
             s->config.geom.x = s2->config.geom.x + s2->config.geom.w;
             s->config.geom.y = s2->config.geom.y +
             ((s2->config.geom.h - s->config.geom.h) *
              s->config.relative.align);
          }
        else if (s->config.relative.mode == E_RANDR2_RELATIVE_TO_ABOVE)
          {
             printf("RRR: to above relative\n");
             s->config.geom.x = s2->config.geom.x +
             ((s2->config.geom.w - s->config.geom.w) *
              s->config.relative.align);
             s->config.geom.y = s2->config.geom.y - s->config.geom.h;
          }
        else if (s->config.relative.mode == E_RANDR2_RELATIVE_TO_BELOW)
          {
             printf("RRR: to below relative\n");
             s->config.geom.x = s2->config.geom.x +
             ((s2->config.geom.w - s->config.geom.w) *
              s->config.relative.align);
             s->config.geom.y = s2->config.geom.y + s2->config.geom.h;
          }
     }
   else
     {
        if ((s->config.relative.mode == E_RANDR2_RELATIVE_CLONE) &&
            (s->config.relative.to))
          {
             E_Config_Randr2_Screen *cs;
             int x = 0, y = 0;

             cs = _config_screen_clone_resolve(e_randr2_cfg,
                                               s->config.relative.to, &x, &y);
             printf("RRR: clone relative - config %p\n", cs);
             if (cs)
               {
                  s->config.geom.x = x;
                  s->config.geom.y = y;
                  s->config.mode.w = cs->mode_w;
                  s->config.mode.h = cs->mode_h;
                  s->config.rotation = cs->rotation;
                  s->config.mode.refresh = cs->mode_refresh;
                  if ((cs->rotation == 0) || (cs->rotation == 180))
                    {
                       s->config.geom.w = s->config.mode.w;
                       s->config.geom.h = s->config.mode.h;
                    }
                  else
                    {
                       s->config.geom.w = s->config.mode.h;
                       s->config.geom.h = s->config.mode.w;
                    }
               }
          }
     }
   _config_do_recurse--;
}

static void
_screen_config_eval(void)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   int minx, miny, maxx, maxy;

   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (s->config.configured) _screen_config_do(s);
     }
   minx = 65535;
   miny = 65535;
   maxx = -65536;
   maxy = -65536;
   printf("RRR:--------------------------------\n");
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (!s->config.enabled) continue;
        if (s->config.geom.x < minx) minx = s->config.geom.x;
        if (s->config.geom.y < miny) miny = s->config.geom.y;
        if ((s->config.geom.x + s->config.geom.w) > maxx)
          maxx = s->config.geom.x + s->config.geom.w;
        if ((s->config.geom.y + s->config.geom.h) > maxy)
          maxy = s->config.geom.y + s->config.geom.h;
        printf("RRR: s: '%s' @ %i %i - %ix%i\n",
               s->info.name,
               s->config.geom.x, s->config.geom.y,
               s->config.geom.w, s->config.geom.h);
     }
   printf("RRR:--- %i %i -> %i %i\n", minx, miny, maxx, maxy);
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        s->config.geom.x -= minx;
        s->config.geom.y -= miny;
     }
   e_randr2->w = maxx - minx;
   e_randr2->h = maxy - miny;
}

static void
_screen_config_maxsize(void)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   int maxx, maxy;

   maxx = -65536;
   maxy = -65536;
   printf("RRR:-------------------------------- 2\n");
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (!s->config.enabled) continue;
        if ((s->config.geom.x + s->config.geom.w) > maxx)
          maxx = s->config.geom.x + s->config.geom.w;
        if ((s->config.geom.y + s->config.geom.h) > maxy)
          maxy = s->config.geom.y + s->config.geom.h;
        printf("RRR: '%s': %i %i %ix%i\n",
               s->info.name,
               s->config.geom.x, s->config.geom.y,
               s->config.geom.w, s->config.geom.h);
     }
   printf("RRR: result max: %ix%i\n", maxx, maxy);
   e_randr2->w = maxx;
   e_randr2->h = maxy;
}

static int
_screen_sort_cb(const void *data1, const void *data2)
{
   const E_Randr2_Screen *s1 = data1, *s2 = data2;
   int dif;

   dif = -(s1->config.priority - s2->config.priority);
   if (dif == 0)
     {
        dif = s1->config.geom.x - s2->config.geom.x;
        if (dif == 0)
          dif = s1->config.geom.y - s2->config.geom.y;
     }
   return dif;
}

EAPI void
e_randr2_screen_refresh_queue(Eina_Bool lid_event)
{
   // delay handling of screen shances as they can come in in a series over
   // time and thus we can batch up responding to them by waiting 1.0 sec
   if (_screen_delay_timer)
     ecore_timer_reset(_screen_delay_timer);
   else
     _screen_delay_timer = ecore_timer_add(1.0, _cb_screen_change_delay, NULL);
   event_screen |= !!lid_event;
}

EAPI E_Config_Randr2_Screen *
e_randr2_config_screen_find(E_Randr2_Screen *s, E_Config_Randr2 *cfg)
{
   Eina_List *l;
   E_Config_Randr2_Screen *cs;

   if ((!s) || (!cfg)) return NULL;
   if (!s->id) return NULL;
   EINA_LIST_FOREACH(cfg->screens, l, cs)
     {
        if (!cs->id) continue;
        if (!strcmp(cs->id, s->id)) return cs;
     }
   return NULL;
}

EAPI void
e_randr2_screens_setup(int rw, int rh)
{
   int i;
   E_Screen *screen;
   Eina_List *screens = NULL, *screens_rem;
   Eina_List *all_screens = NULL;
   Eina_List *l, *ll;
   E_Randr2_Screen *s, *s2, *s_chosen;
   Eina_Bool removed;

   if ((!e_randr2) || (!e_randr2->screens)) goto out;
   // put screens in tmp list
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if ((s->config.enabled) &&
            (s->config.geom.w > 0) &&
            (s->config.geom.h > 0))
          {
             screens = eina_list_append(screens, s);
          }
     }
   // remove overlapping screens - if a set of screens overlap, keep the
   // smallest/lowest res
   do
     {
        removed = EINA_FALSE;

        EINA_LIST_FOREACH(screens, l, s)
          {
             screens_rem = NULL;

             EINA_LIST_FOREACH(l->next, ll, s2)
               {
                  if (E_INTERSECTS(s->config.geom.x, s->config.geom.y,
                                   s->config.geom.w, s->config.geom.h,
                                   s2->config.geom.x, s2->config.geom.y,
                                   s2->config.geom.w, s2->config.geom.h))
                    {
                       if (!screens_rem)
                         screens_rem = eina_list_append(screens_rem, s);
                       screens_rem = eina_list_append(screens_rem, s2);
                    }
               }
             // we have intersecting screens - choose the lowest res one
             if (screens_rem)
               {
                  removed = EINA_TRUE;
                  // find the smallest screen (chosen one)
                  s_chosen = NULL;
                  EINA_LIST_FOREACH(screens_rem, ll, s2)
                    {
                       if (!s_chosen) s_chosen = s2;
                       else
                         {
                            if ((s_chosen->config.geom.w *
                                 s_chosen->config.geom.h) >
                                (s2->config.geom.w *
                                 s2->config.geom.h))
                              s_chosen = s2;
                         }
                    }
                  // remove all from screens but the chosen one
                  EINA_LIST_FREE(screens_rem, s2)
                    {
                       if (s2 != s_chosen)
                         screens = eina_list_remove_list(screens, l);
                    }
                  // break our list walk and try again
                  break;
               }
          }
     }
   while (removed);
   // sort screens by priority etc.
   screens = eina_list_sort(screens, 0, _screen_sort_cb);
   i = 0;
   EINA_LIST_FOREACH(screens, l, s)
     {
        screen = E_NEW(E_Screen, 1);
        screen->escreen = screen->screen = i;
        screen->x = s->config.geom.x;
        screen->y = s->config.geom.y;
        screen->w = s->config.geom.w;
        screen->h = s->config.geom.h;
        all_screens = eina_list_append(all_screens, screen);
        printf("xinerama screen %i %i %ix%i\n", screen->x, screen->y, screen->w, screen->h);
        INF("E INIT: XINERAMA SCREEN: [%i][%i], %ix%i+%i+%i",
            i, i, screen->w, screen->h, screen->x, screen->y);
        i++;
     }
   eina_list_free(screens);
   // if we have NO screens at all (above - i will be 0) AND we have no
   // existing screens set up in xinerama - then just say root window size
   // is the entire screen. this should handle the case where you unplug ALL
   // screens from an existing setup (unplug external monitors and/or close
   // laptop lid), in which case as long as at least one screen is configured
   // in xinerama, it will be left-as is until next time we re-eval screen
   // setup and have at least one screen
   printf("xinerama setup............... %i %p\n", i, e_xinerama_screens_all_get());
   if ((i == 0) && (!e_xinerama_screens_all_get()))
     {
out:
        screen = E_NEW(E_Screen, 1);
        screen->escreen = screen->screen = 0;
        screen->x = 0;
        screen->y = 0;
        if ((rw > 0) && (rh > 0))
          screen->w = rw, screen->h = rh;
        else
          ecore_evas_screen_geometry_get(e_comp->ee, NULL, NULL, &screen->w, &screen->h);
        all_screens = eina_list_append(all_screens, screen);
     }
   e_xinerama_screens_set(all_screens);
}
