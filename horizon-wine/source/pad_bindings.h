/*
 * What the controller does in a program that reads the keyboard and mouse:
 * each control, or a combination of two, sends a key (with Shift, Ctrl, Alt or
 * the Windows key held if it says so), a mouse button or a turn of the wheel.
 *
 * keys.txt holds one NAME=action line for each control that differs from the
 * default, and MOD+NAME=action for a combination: L+A=0x54 sends T while L is
 * held and A pressed. An action is a virtual-key code (0x54), the same with
 * modifiers (shift+0x31), mouse:left, right, middle, x1 or x2, wheel:up or
 * down, or none.
 *
 * A combination wants its first control held before the second is pressed.
 * COMBOS=hold (the default) has a control that starts combinations send its
 * own action for as long as it is held, combinations or not: ZR can click and
 * hold the click while ZR+A casts. COMBOS=tap has it wait instead and send
 * its own action only when let go without a combination used meanwhile, as a
 * tap, for a shoulder that is only ever a shift.
 *
 * No libnx here: the runtime (runtime.c) turns the controller into the held
 * controls each poll, the launcher reads and writes the same lines, and the
 * host test (tests/pad_bindings.c) runs it all on a computer.
 */
#ifndef WINE_NX_PAD_BINDINGS_H
#define WINE_NX_PAD_BINDINGS_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* The controls that can be bound, in the order of the runtime's bits. */
enum
{
    WINE_NX_KEY_UP, WINE_NX_KEY_DOWN, WINE_NX_KEY_LEFT, WINE_NX_KEY_RIGHT,
    WINE_NX_KEY_X, WINE_NX_KEY_Y, WINE_NX_KEY_L, WINE_NX_KEY_R,
    WINE_NX_KEY_ZL, WINE_NX_KEY_ZR, WINE_NX_KEY_PLUS, WINE_NX_KEY_MINUS,
    WINE_NX_KEY_STICKL, WINE_NX_KEY_STICKR, WINE_NX_KEY_A, WINE_NX_KEY_B,
    /* Each of the three things that point, for a game that walks with one set
     * of keys and works its menus with another. The left stick sends what the
     * d-pad does until it is given keys of its own. */
    WINE_NX_KEY_LUP, WINE_NX_KEY_LDOWN, WINE_NX_KEY_LLEFT, WINE_NX_KEY_LRIGHT,
    WINE_NX_KEY_RUP, WINE_NX_KEY_RDOWN, WINE_NX_KEY_RLEFT, WINE_NX_KEY_RRIGHT,
    WINE_NX_KEY_TUP, WINE_NX_KEY_TDOWN, WINE_NX_KEY_TLEFT, WINE_NX_KEY_TRIGHT,
    WINE_NX_KEY_COUNT
};

#define PAD_BIND_COMBO_MAX 64

enum pad_action_type
{
    PAD_ACTION_UNSET,   /* no line: the control's default (A clicks, the left stick follows the d-pad) */
    PAD_ACTION_NONE,    /* sends nothing */
    PAD_ACTION_KEY,     /* code is a virtual key, mods the modifiers held with it */
    PAD_ACTION_MOUSE,   /* code is a PAD_MOUSE_* button */
    PAD_ACTION_WHEEL,   /* code is PAD_WHEEL_UP or PAD_WHEEL_DOWN */
};

#define PAD_MOD_SHIFT 0x1
#define PAD_MOD_CTRL  0x2
#define PAD_MOD_ALT   0x4
#define PAD_MOD_WIN   0x8

enum { PAD_MOUSE_LEFT = 1, PAD_MOUSE_RIGHT, PAD_MOUSE_MIDDLE, PAD_MOUSE_X1, PAD_MOUSE_X2, PAD_MOUSE_COUNT };
enum { PAD_WHEEL_UP = 1, PAD_WHEEL_DOWN };

/* The mouse buttons as bits, as the runtime's pointer reports them. */
#define PAD_MOUSE_BIT(button) (1u << ((button) - 1))

/* How long a wheel action held waits before it turns again, and then how
 * often it does: a held shoulder scrolls a list or zooms a map. */
#define PAD_WHEEL_DELAY_NS  350000000ull
#define PAD_WHEEL_REPEAT_NS 120000000ull

struct pad_action
{
    unsigned char type, code, mods;
};

struct pad_combo
{
    signed char mod, source;   /* hold mod, then press source */
    struct pad_action action;
};

struct pad_bindings
{
    struct pad_action single[WINE_NX_KEY_COUNT];
    struct pad_combo combo[PAD_BIND_COMBO_MAX];
    int combos;
    int modifier_tap;          /* COMBOS=tap: a control starting combinations waits to be tapped */
};

/* The COMBOS line's value. Returns 0 when it is neither word. */
static inline int pad_combos_mode_parse( const char *text, int *tap )
{
    if (!strcasecmp( text, "hold" )) *tap = 0;
    else if (!strcasecmp( text, "tap" )) *tap = 1;
    else return 0;
    return 1;
}

/* What a step leaves held, and what was pressed and let go within it. Keys are
 * a bit for each virtual-key code. */
struct pad_bind_output
{
    unsigned int keys[8], keys_tapped[8];
    unsigned int mouse, mouse_tapped;
    int wheel;                 /* notches, up positive */
};

#define PAD_EMIT_NOTHING (-3)
#define PAD_EMIT_PENDING (-2)  /* a modifier, its own action waiting for it to be let go */
#define PAD_EMIT_SINGLE  (-1)  /* its own action; >= 0 is the combination it started */

struct pad_bind_state
{
    unsigned int held;         /* the controls held at the last step */
    unsigned int used;         /* modifiers that have started a combination since pressed */
    signed char emitting[WINE_NX_KEY_COUNT];
    unsigned int order[WINE_NX_KEY_COUNT];
    unsigned int sequence;
    unsigned long long wheel_next[WINE_NX_KEY_COUNT];
};

static const char *const pad_mouse_names[PAD_MOUSE_COUNT] = { NULL, "left", "right", "middle", "x1", "x2" };

static inline void pad_bind_state_reset( struct pad_bind_state *s )
{
    int i;

    memset( s, 0, sizeof(*s) );
    for (i = 0; i < WINE_NX_KEY_COUNT; i++) s->emitting[i] = PAD_EMIT_NOTHING;
}

/* Every control sends its default and there are no combinations. defaults[]
 * is the runtime's virtual-key code for each control, 0 for none. */
static inline void pad_bindings_init( struct pad_bindings *b, const unsigned short *defaults )
{
    int i;

    memset( b, 0, sizeof(*b) );
    for (i = 0; i < WINE_NX_KEY_COUNT; i++)
    {
        if (!defaults || !defaults[i]) continue;
        b->single[i].type = PAD_ACTION_KEY;
        b->single[i].code = (unsigned char)defaults[i];
    }
}

/* ---- Lines ------------------------------------------------------------ */

/* An action as keys.txt spells it. Returns 0 when the text is not one. A bare
 * 0 is the control's default, which is what earlier files said with it. */
static inline int pad_action_parse( const char *text, struct pad_action *out )
{
    struct pad_action a = {0};
    char buffer[64], *part, *next, *end;
    unsigned long code;
    int i;

    while (*text == ' ' || *text == '\t') text++;
    if (strlen( text ) >= sizeof(buffer)) return 0;
    strcpy( buffer, text );
    for (end = buffer + strlen( buffer ); end > buffer && (end[-1] == ' ' || end[-1] == '\t'); end--) end[-1] = 0;
    if (!buffer[0]) return 0;

    if (!strcasecmp( buffer, "none" ))
    {
        a.type = PAD_ACTION_NONE;
        *out = a;
        return 1;
    }
    if (!strncasecmp( buffer, "mouse:", 6 ))
    {
        for (i = 1; i < PAD_MOUSE_COUNT; i++)
            if (!strcasecmp( buffer + 6, pad_mouse_names[i] )) break;
        if (i == PAD_MOUSE_COUNT) return 0;
        a.type = PAD_ACTION_MOUSE;
        a.code = (unsigned char)i;
        *out = a;
        return 1;
    }
    if (!strncasecmp( buffer, "wheel:", 6 ))
    {
        if (!strcasecmp( buffer + 6, "up" )) a.code = PAD_WHEEL_UP;
        else if (!strcasecmp( buffer + 6, "down" )) a.code = PAD_WHEEL_DOWN;
        else return 0;
        a.type = PAD_ACTION_WHEEL;
        *out = a;
        return 1;
    }
    /* [shift+][ctrl+][alt+][win+]code */
    for (part = buffer; (next = strchr( part, '+' )); part = next + 1)
    {
        *next = 0;
        if (!strcasecmp( part, "shift" )) a.mods |= PAD_MOD_SHIFT;
        else if (!strcasecmp( part, "ctrl" )) a.mods |= PAD_MOD_CTRL;
        else if (!strcasecmp( part, "alt" )) a.mods |= PAD_MOD_ALT;
        else if (!strcasecmp( part, "win" )) a.mods |= PAD_MOD_WIN;
        else return 0;
    }
    code = strtoul( part, &end, 0 );
    if (end == part || *end || code > 0xff) return 0;
    a.type = code || a.mods ? PAD_ACTION_KEY : PAD_ACTION_UNSET;
    a.code = (unsigned char)code;
    *out = a;
    return 1;
}

/* The line's value for an action; the inverse of pad_action_parse. */
static inline const char *pad_action_format( const struct pad_action *a, char *out, size_t size )
{
    switch (a->type)
    {
    case PAD_ACTION_NONE: snprintf( out, size, "none" ); break;
    case PAD_ACTION_MOUSE:
        snprintf( out, size, "mouse:%s", a->code && a->code < PAD_MOUSE_COUNT ? pad_mouse_names[a->code] : "left" );
        break;
    case PAD_ACTION_WHEEL: snprintf( out, size, "wheel:%s", a->code == PAD_WHEEL_DOWN ? "down" : "up" ); break;
    case PAD_ACTION_KEY:
        snprintf( out, size, "%s%s%s%s0x%02x", a->mods & PAD_MOD_SHIFT ? "shift+" : "",
                  a->mods & PAD_MOD_CTRL ? "ctrl+" : "", a->mods & PAD_MOD_ALT ? "alt+" : "",
                  a->mods & PAD_MOD_WIN ? "win+" : "", a->code );
        break;
    default: snprintf( out, size, "0" ); break;
    }
    return out;
}

/* A control by its name in names[], -1 for none. */
static inline int pad_control_index( const char *name, size_t length, const char *const *names, int count )
{
    int i;

    for (i = 0; i < count; i++)
        if (strlen( names[i] ) == length && !strncasecmp( name, names[i], length )) return i;
    return -1;
}

/* NAME or MOD+NAME. Returns 0 when it names no control; *mod is -1 for one alone. */
static inline int pad_trigger_parse( const char *text, const char *const *names, int count, int *mod, int *source )
{
    const char *plus = strchr( text, '+' );

    if (!plus)
    {
        *mod = -1;
        return (*source = pad_control_index( text, strlen( text ), names, count )) >= 0;
    }
    *mod = pad_control_index( text, plus - text, names, count );
    *source = pad_control_index( plus + 1, strlen( plus + 1 ), names, count );
    return *mod >= 0 && *source >= 0 && *mod != *source;
}

/* Give a control, or a combination, an action, over what it had. Returns 0
 * when there is no room for another combination. */
static inline int pad_bindings_set( struct pad_bindings *b, int mod, int source, const struct pad_action *a )
{
    int i;

    if (source < 0 || source >= WINE_NX_KEY_COUNT || mod >= WINE_NX_KEY_COUNT) return 0;
    if (mod < 0)
    {
        b->single[source] = *a;
        return 1;
    }
    for (i = 0; i < b->combos; i++)
        if (b->combo[i].mod == mod && b->combo[i].source == source)
        {
            b->combo[i].action = *a;
            return 1;
        }
    if (b->combos == PAD_BIND_COMBO_MAX) return 0;
    b->combo[b->combos].mod = (signed char)mod;
    b->combo[b->combos].source = (signed char)source;
    b->combo[b->combos].action = *a;
    b->combos++;
    return 1;
}

/* ---- Steps ------------------------------------------------------------ */

static inline int pad_combo_live( const struct pad_combo *c )
{
    return c->action.type != PAD_ACTION_NONE && c->action.type != PAD_ACTION_UNSET;
}

/* The controls that start a combination. */
static inline unsigned int pad_bindings_modifiers( const struct pad_bindings *b )
{
    unsigned int mods = 0;
    int i;

    for (i = 0; i < b->combos; i++)
        if (pad_combo_live( &b->combo[i] )) mods |= 1u << b->combo[i].mod;
    return mods;
}

/* The control whose bindings a control uses: a direction of the left stick
 * with no action of its own is the d-pad's. */
static inline int pad_bindings_resolve( const struct pad_bindings *b, int source )
{
    if (source >= WINE_NX_KEY_LUP && source <= WINE_NX_KEY_LRIGHT &&
        b->single[source].type == PAD_ACTION_UNSET)
        return WINE_NX_KEY_UP + source - WINE_NX_KEY_LUP;
    return source;
}

/* A control's own action, with its default filled in. */
static inline struct pad_action pad_bindings_single( const struct pad_bindings *b, int source )
{
    struct pad_action a = b->single[pad_bindings_resolve( b, source )];

    if (a.type != PAD_ACTION_UNSET) return a;
    if (source == WINE_NX_KEY_A || source == WINE_NX_KEY_B)
    {
        a.type = PAD_ACTION_MOUSE;
        a.code = source == WINE_NX_KEY_A ? PAD_MOUSE_LEFT : PAD_MOUSE_RIGHT;
        return a;
    }
    a.type = PAD_ACTION_NONE;
    return a;
}

static inline void pad_output_key( unsigned int *keys, unsigned int code )
{
    keys[(code >> 5) & 7] |= 1u << (code & 31);
}

static inline int pad_output_has_key( const unsigned int *keys, unsigned int code )
{
    return !!(keys[(code >> 5) & 7] & (1u << (code & 31)));
}

/* Add an action to what the step leaves held, or to what it taps. */
static inline void pad_output_add( struct pad_bind_output *out, const struct pad_action *a, int tap )
{
    unsigned int *keys = tap ? out->keys_tapped : out->keys;

    switch (a->type)
    {
    case PAD_ACTION_KEY:
        if (a->code) pad_output_key( keys, a->code );
        if (a->mods & PAD_MOD_SHIFT) pad_output_key( keys, 0x10 );
        if (a->mods & PAD_MOD_CTRL) pad_output_key( keys, 0x11 );
        if (a->mods & PAD_MOD_ALT) pad_output_key( keys, 0x12 );
        if (a->mods & PAD_MOD_WIN) pad_output_key( keys, 0x5b );
        break;
    case PAD_ACTION_MOUSE:
        if (a->code && a->code < PAD_MOUSE_COUNT)
            *(tap ? &out->mouse_tapped : &out->mouse) |= PAD_MOUSE_BIT(a->code);
        break;
    case PAD_ACTION_WHEEL:
        if (tap) out->wheel += a->code == PAD_WHEEL_DOWN ? -1 : 1;
        break;
    }
}

static inline struct pad_action pad_bind_emitting( const struct pad_bindings *b, const struct pad_bind_state *s,
                                                   int source )
{
    static const struct pad_action nothing = { PAD_ACTION_NONE, 0, 0 };

    if (s->emitting[source] == PAD_EMIT_SINGLE) return pad_bindings_single( b, source );
    if (s->emitting[source] >= 0 && s->emitting[source] < b->combos) return b->combo[s->emitting[source]].action;
    return nothing;
}

/* Take the controls held now (a bit for each) to what they send. */
static inline void pad_bind_step( struct pad_bind_state *s, const struct pad_bindings *b, unsigned int held,
                                  unsigned long long now_ns, struct pad_bind_output *out )
{
    unsigned int mods = pad_bindings_modifiers( b );
    unsigned int pressed = held & ~s->held, released = s->held & ~held;
    int i, pass;

    memset( out, 0, sizeof(*out) );

    for (i = 0; i < WINE_NX_KEY_COUNT; i++)
    {
        if (!(released & (1u << i))) continue;
        /* A modifier let go without a combination: its own action, as a tap. */
        if (s->emitting[i] == PAD_EMIT_PENDING && !(s->used & (1u << i)))
        {
            struct pad_action a = pad_bindings_single( b, i );
            pad_output_add( out, &a, 1 );
        }
        s->emitting[i] = PAD_EMIT_NOTHING;
        s->used &= ~(1u << i);
    }

    /* Modifiers first, so a combination pressed within one poll still counts. */
    for (pass = 0; pass < 2; pass++)
        for (i = 0; i < WINE_NX_KEY_COUNT; i++)
        {
            int source = pad_bindings_resolve( b, i ), best = -1, c;
            unsigned int best_order = 0;
            struct pad_action a;

            if (!(pressed & (1u << i)) || !!(mods & (1u << i)) != (pass == 0)) continue;
            s->order[i] = ++s->sequence;
            /* The combination of the modifier pressed last, of those held. */
            for (c = 0; c < b->combos; c++)
            {
                int mod = b->combo[c].mod;

                if (b->combo[c].source != source || !pad_combo_live( &b->combo[c] )) continue;
                if (mod == i || !(held & (1u << mod)) || s->order[mod] >= s->order[i]) continue;
                if (best < 0 || s->order[mod] > best_order)
                {
                    best = c;
                    best_order = s->order[mod];
                }
            }
            if (best >= 0)
            {
                s->emitting[i] = (signed char)best;
                s->used |= 1u << b->combo[best].mod;
            }
            else s->emitting[i] = b->modifier_tap && (mods & (1u << i)) ? PAD_EMIT_PENDING : PAD_EMIT_SINGLE;

            a = pad_bind_emitting( b, s, i );
            if (a.type == PAD_ACTION_WHEEL)
            {
                pad_output_add( out, &a, 1 );
                s->wheel_next[i] = now_ns + PAD_WHEEL_DELAY_NS;
            }
        }

    for (i = 0; i < WINE_NX_KEY_COUNT; i++)
    {
        struct pad_action a;

        if (!(held & (1u << i))) continue;
        a = pad_bind_emitting( b, s, i );
        /* The wheel turns once when pressed (above), then again while held. */
        if (a.type == PAD_ACTION_WHEEL && !(pressed & (1u << i)) && now_ns >= s->wheel_next[i])
        {
            pad_output_add( out, &a, 1 );
            s->wheel_next[i] = now_ns + PAD_WHEEL_REPEAT_NS;
        }
        pad_output_add( out, &a, 0 );
    }
    s->held = held;
}

#endif /* WINE_NX_PAD_BINDINGS_H */
