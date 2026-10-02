/* Host test for what the controller sends (horizon-wine/source/pad_bindings.h):
 * the keys.txt lines, combinations, taps, mouse buttons and the wheel. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../source/pad_bindings.h"

static const char *const names[WINE_NX_KEY_COUNT] =
{
    "UP", "DOWN", "LEFT", "RIGHT", "X", "Y", "L", "R",
    "ZL", "ZR", "PLUS", "MINUS", "STICKL", "STICKR", "A", "B",
    "LUP", "LDOWN", "LLEFT", "LRIGHT",
    "RUP", "RDOWN", "RLEFT", "RRIGHT",
    "TUP", "TDOWN", "TLEFT", "TRIGHT"
};

/* The runtime's defaults (runtime.c, wine_nx_pad_keys). */
static const unsigned short defaults[WINE_NX_KEY_COUNT] =
{
    0x26, 0x28, 0x25, 0x27, 0x20, 0x46, 0x09, 0x10, 0x28, 0x26, 0x1b, 0x09, 0x11, 0x12, 0, 0,
    0, 0, 0, 0, 0x26, 0x28, 0x25, 0x27, 0x26, 0x28, 0x25, 0x27,
};

#define BIT(control) (1u << WINE_NX_KEY_##control)
#define MS 1000000ull

static struct pad_bindings bindings;
static struct pad_bind_state state;
static struct pad_bind_output out;

static void line( const char *trigger, const char *action )
{
    struct pad_action a;
    int mod, source;

    assert( pad_trigger_parse( trigger, names, WINE_NX_KEY_COUNT, &mod, &source ) );
    assert( pad_action_parse( action, &a ) );
    assert( pad_bindings_set( &bindings, mod, source, &a ) );
}

static void fresh( void )
{
    pad_bindings_init( &bindings, defaults );
    pad_bind_state_reset( &state );
}

static void step( unsigned int held, unsigned long long now )
{
    pad_bind_step( &state, &bindings, held, now, &out );
}

static int key( unsigned int code ) { return pad_output_has_key( out.keys, code ); }
static int tapped( unsigned int code ) { return pad_output_has_key( out.keys_tapped, code ); }

static int no_keys( void )
{
    int i;

    for (i = 0; i < 8; i++) if (out.keys[i] || out.keys_tapped[i]) return 0;
    return !out.mouse && !out.mouse_tapped && !out.wheel;
}

static void test_lines( void )
{
    struct pad_action a;
    char text[64];
    int mod, source;

    assert( pad_action_parse( "0x54", &a ) && a.type == PAD_ACTION_KEY && a.code == 0x54 && !a.mods );
    assert( pad_action_parse( " 84 ", &a ) && a.type == PAD_ACTION_KEY && a.code == 0x54 );
    assert( !strcmp( pad_action_format( &a, text, sizeof(text) ), "0x54" ) );
    assert( pad_action_parse( "Shift+ctrl+0x31", &a ) && a.code == 0x31 && a.mods == (PAD_MOD_SHIFT | PAD_MOD_CTRL) );
    assert( !strcmp( pad_action_format( &a, text, sizeof(text) ), "shift+ctrl+0x31" ) );
    assert( pad_action_parse( "mouse:x1", &a ) && a.type == PAD_ACTION_MOUSE && a.code == PAD_MOUSE_X1 );
    assert( !strcmp( pad_action_format( &a, text, sizeof(text) ), "mouse:x1" ) );
    assert( pad_action_parse( "wheel:down", &a ) && a.type == PAD_ACTION_WHEEL && a.code == PAD_WHEEL_DOWN );
    assert( !strcmp( pad_action_format( &a, text, sizeof(text) ), "wheel:down" ) );
    assert( pad_action_parse( "none", &a ) && a.type == PAD_ACTION_NONE );
    /* A bare 0 is the default, as earlier files wrote it for A and B. */
    assert( pad_action_parse( "0x00", &a ) && a.type == PAD_ACTION_UNSET );
    assert( !pad_action_parse( "", &a ) );
    assert( !pad_action_parse( "mouse:side", &a ) );
    assert( !pad_action_parse( "0x100", &a ) );
    assert( !pad_action_parse( "shift+", &a ) );
    assert( !pad_action_parse( "hyper+0x31", &a ) );
    assert( !pad_action_parse( "0x31junk", &a ) );

    assert( pad_trigger_parse( "a", names, WINE_NX_KEY_COUNT, &mod, &source ) && mod == -1 && source == WINE_NX_KEY_A );
    assert( pad_trigger_parse( "L+A", names, WINE_NX_KEY_COUNT, &mod, &source ) &&
            mod == WINE_NX_KEY_L && source == WINE_NX_KEY_A );
    assert( !pad_trigger_parse( "L+L", names, WINE_NX_KEY_COUNT, &mod, &source ) );
    assert( !pad_trigger_parse( "L+", names, WINE_NX_KEY_COUNT, &mod, &source ) );
    assert( !pad_trigger_parse( "Q", names, WINE_NX_KEY_COUNT, &mod, &source ) );
    assert( !pad_trigger_parse( "LA", names, WINE_NX_KEY_COUNT, &mod, &source ) );
}

static void test_defaults( void )
{
    fresh();
    step( BIT(A), 0 );
    assert( out.mouse == PAD_MOUSE_BIT(PAD_MOUSE_LEFT) );
    step( BIT(A) | BIT(B) | BIT(X), MS );
    assert( out.mouse == (PAD_MOUSE_BIT(PAD_MOUSE_LEFT) | PAD_MOUSE_BIT(PAD_MOUSE_RIGHT)) && key( 0x20 ) );
    step( 0, 2 * MS );
    assert( no_keys() );
    /* The left stick with no keys of its own sends the d-pad's. */
    step( BIT(LUP), 3 * MS );
    assert( key( 0x26 ) );
    line( "LUP", "0x57" );
    step( BIT(LUP), 4 * MS );
    assert( key( 0x57 ) && !key( 0x26 ) );
    /* A given a key no longer clicks; given none it sends nothing. */
    line( "A", "0x0d" );
    line( "B", "none" );
    step( BIT(A) | BIT(B), 5 * MS );
    assert( key( 0x0d ) && !out.mouse );
    /* Any control can click. */
    line( "ZR", "mouse:left" );
    line( "ZL", "mouse:middle" );
    step( BIT(ZR) | BIT(ZL), 6 * MS );
    assert( out.mouse == (PAD_MOUSE_BIT(PAD_MOUSE_LEFT) | PAD_MOUSE_BIT(PAD_MOUSE_MIDDLE)) );
}

static void test_combinations( void )
{
    fresh();
    line( "L+A", "0x54" );
    line( "L+X", "shift+0x31" );

    /* L starts combinations, so it sends nothing while held... */
    step( BIT(L), 0 );
    assert( no_keys() );
    /* ...and L+A is T, without A's click. */
    step( BIT(L) | BIT(A), MS );
    assert( key( 0x54 ) && !out.mouse );
    /* Letting go of L first keeps A on T until A is let go. */
    step( BIT(A), 2 * MS );
    assert( key( 0x54 ) && !out.mouse );
    step( 0, 3 * MS );
    /* No tap of L's Tab: it was used. */
    assert( no_keys() );

    /* L alone, let go, is its Tab as a tap. */
    step( BIT(L), 4 * MS );
    assert( no_keys() );
    step( 0, 5 * MS );
    assert( tapped( 0x09 ) && !key( 0x09 ) );

    /* A first, then L: no combination; A clicks, and L still taps. */
    step( BIT(A), 6 * MS );
    step( BIT(A) | BIT(L), 7 * MS );
    assert( out.mouse == PAD_MOUSE_BIT(PAD_MOUSE_LEFT) && !key( 0x54 ) );
    step( BIT(A), 8 * MS );
    assert( tapped( 0x09 ) );
    step( 0, 9 * MS );

    /* Both within one poll count as the combination. */
    step( BIT(L) | BIT(A), 10 * MS );
    assert( key( 0x54 ) );
    step( 0, 11 * MS );
    assert( no_keys() );

    /* A combination with modifiers holds them with its key. */
    step( BIT(L), 12 * MS );
    step( BIT(L) | BIT(X), 13 * MS );
    assert( key( 0x10 ) && key( 0x31 ) && !key( 0x20 ) );
    step( 0, 14 * MS );

    /* X, which starts nothing, still sends Space while held. */
    step( BIT(X), 15 * MS );
    assert( key( 0x20 ) );
    step( 0, 16 * MS );

    /* Of two modifiers held, the one pressed last. */
    line( "R+A", "0x59" );
    step( BIT(L), 17 * MS );
    step( BIT(L) | BIT(R), 18 * MS );
    step( BIT(L) | BIT(R) | BIT(A), 19 * MS );
    assert( key( 0x59 ) && !key( 0x54 ) );
    step( 0, 20 * MS );

    /* A combination set to none no longer makes its first control a modifier. */
    fresh();
    line( "L+A", "0x54" );
    line( "L+A", "none" );
    assert( bindings.combos == 1 );
    step( BIT(L), 0 );
    assert( key( 0x09 ) );
    step( 0, MS );

    /* The left stick following the d-pad follows its combinations too. */
    fresh();
    line( "L+UP", "0x31" );
    step( BIT(L), 0 );
    step( BIT(L) | BIT(LUP), MS );
    assert( key( 0x31 ) && !key( 0x26 ) );
    step( 0, 2 * MS );
}

static void test_wheel( void )
{
    fresh();
    line( "ZL", "wheel:up" );
    line( "R+ZL", "wheel:down" );
    step( BIT(ZL), 0 );
    assert( out.wheel == 1 );
    step( BIT(ZL), 100 * MS );
    assert( out.wheel == 0 );
    step( BIT(ZL), PAD_WHEEL_DELAY_NS );
    assert( out.wheel == 1 );
    step( BIT(ZL), PAD_WHEEL_DELAY_NS + 50 * MS );
    assert( out.wheel == 0 );
    step( BIT(ZL), PAD_WHEEL_DELAY_NS + PAD_WHEEL_REPEAT_NS );
    assert( out.wheel == 1 );
    step( 0, 1000 * MS );
    assert( no_keys() );
    step( BIT(R), 1100 * MS );
    step( BIT(R) | BIT(ZL), 1200 * MS );
    assert( out.wheel == -1 );
    step( 0, 1300 * MS );
}

static void test_room( void )
{
    static const int mods[] = { WINE_NX_KEY_R, WINE_NX_KEY_ZL, WINE_NX_KEY_ZR };
    struct pad_action a = { PAD_ACTION_KEY, 0x41, 0 };
    int i;

    fresh();
    for (i = 0; bindings.combos < PAD_BIND_COMBO_MAX; i++)
    {
        int mod = mods[i / WINE_NX_KEY_COUNT], source = i % WINE_NX_KEY_COUNT;

        if (source != mod) assert( pad_bindings_set( &bindings, mod, source, &a ) );
    }
    /* Full: no new one, but one that is there can still change. */
    assert( !pad_bindings_set( &bindings, WINE_NX_KEY_X, WINE_NX_KEY_Y, &a ) );
    a.code = 0x42;
    assert( pad_bindings_set( &bindings, WINE_NX_KEY_R, WINE_NX_KEY_UP, &a ) );
    assert( bindings.combo[0].action.code == 0x42 && bindings.combos == PAD_BIND_COMBO_MAX );
}

int main( void )
{
    test_lines();
    test_defaults();
    test_combinations();
    test_wheel();
    test_room();
    printf( "pad bindings: lines, defaults, combinations, wheel\n" );
    return 0;
}
