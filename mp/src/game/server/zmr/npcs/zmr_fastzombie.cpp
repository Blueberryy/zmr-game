#include "cbase.h"
#include "ai_basenpc.h"
#include "ai_default.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_motor.h"
#include "ai_memory.h"
#include "ai_route.h"
#include "soundent.h"
#include "game.h"
#include "npcevent.h"
#include "entitylist.h"
#include "ai_task.h"
#include "activitylist.h"
#include "engine/IEngineSound.h"
#include "movevars_shared.h"
#include "IEffects.h"
#include "props.h"
#include "physics_npc_solver.h"

//#include "hl2/npc_basezombie.h"


#include "zmr/zmr_gamerules.h"
#include "zmr_fastzombie.h"
#include "zmr_zombiebase.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//TGB: clinging stuff
#define FASTZOMBIE_CLING_DETECTRANGE 256
#define FASTZOMBIE_CLING_MAXHEIGHT	 375
#define FASTZOMBIE_CLING_JUMPSPEED	 700

// unused?
#define FASTZOMBIE_IDLE_PITCH			35
#define FASTZOMBIE_MIN_PITCH			70
#define FASTZOMBIE_MAX_PITCH			130
#define FASTZOMBIE_SOUND_UPDATE_FREQ	0.5

#define FASTZOMBIE_EXCITE_DIST 480.0

#define FASTZOMBIE_BASE_FREQ 1.5

// If flying at an enemy, and this close or closer, start playing the maul animation!!
#define FASTZOMBIE_MAUL_RANGE	300


extern ConVar zm_sk_banshee_health;
extern ConVar zm_sk_banshee_dmg_claw;
extern ConVar zm_sk_banshee_dmg_leap;
//static ConVar zm_popcost_banshee("zm_popcost_banshee", "5", FCVAR_NOTIFY | FCVAR_REPLICATED, "Population points taken up by banshees");

static ConVar sv_climbing( "sv_climbing", "1" );


//=========================================================
// animation events
//=========================================================
int AE_FASTZOMBIE_LEAP;
int AE_FASTZOMBIE_GALLOP_LEFT;
int AE_FASTZOMBIE_GALLOP_RIGHT;
int AE_FASTZOMBIE_CLIMB_LEFT;
int AE_FASTZOMBIE_CLIMB_RIGHT;

//=========================================================
// tasks
//=========================================================


//=========================================================
// activities
//=========================================================
int ACT_FASTZOMBIE_LEAP_SOAR;
int ACT_FASTZOMBIE_LEAP_STRIKE;
int ACT_FASTZOMBIE_LAND_RIGHT;
int ACT_FASTZOMBIE_LAND_LEFT;
int ACT_FASTZOMBIE_FRENZY;
int ACT_FASTZOMBIE_BIG_SLASH;

//=========================================================
// schedules
//=========================================================
//TGB: moved to basezombie for easier access


//=========================================================
//=========================================================
class CFastZombie : public CAI_BlendingHost<CZMBaseZombie>
{
public:
    DECLARE_CLASS( CFastZombie, CAI_BlendingHost<CZMBaseZombie> )
    DECLARE_SERVERCLASS()
    DECLARE_DATADESC()
    DEFINE_CUSTOM_AI

    CFastZombie( void );
    ~CFastZombie( void );


    void Spawn( void );
    void Precache( void );

    virtual void SetZombieModel( void ) OVERRIDE;

    int	TranslateSchedule( int scheduleType );

    Activity NPC_TranslateActivity( Activity baseAct );

    void LeapAttackTouch( CBaseEntity *pOther );
    void ClimbTouch( CBaseEntity *pOther );


    //TGB: zoomj ceiling cling test
    void			CeilingJumpTouch( CBaseEntity *pOther );

    CBaseEntity*	GetClingAmbushTarget();
    static bool		IsCeilingFlat( Vector plane_normal );
    
    void			CeilingDetach() {
        SetMoveType( MOVETYPE_STEP );
        SetGroundEntity(NULL);
        m_bClinging = false;
    }	


    void StartTask( const Task_t *pTask );
    void RunTask( const Task_t *pTask );
    int SelectSchedule( void );
    void OnScheduleChange( void );

    void PrescheduleThink( void );

    int RangeAttack1Conditions( float flDot, float flDist );
    int MeleeAttack1Conditions( float flDot, float flDist );

    virtual float GetClawAttackRange() const { return 50; }

    bool ShouldPlayFootstepMoan( void ) { return false; }

    void HandleAnimEvent( animevent_t *pEvent );

    void PostNPCInit( void );

    void LeapAttack( void );
    void LeapAttackSound( void );

    bool IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const;
    bool MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost );
    bool ShouldFailNav( bool bMovementFailed );

    int	SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );

    const char *GetMoanSound( int nSound );

    void OnChangeActivity( Activity NewActivity );
    void OnStateChange( NPC_STATE OldState, NPC_STATE NewState );
    void Event_Killed( const CTakeDamageInfo &info );

    void PainSound( const CTakeDamageInfo &info );
    void DeathSound( const CTakeDamageInfo &info ); 
    void AlertSound( void );
    void IdleSound( void );
    void AttackSound( void );
    void AttackHitSound( void );
    void AttackMissSound( void );
    void FootstepSound( bool fRightFoot );
    void FootscuffSound( bool fRightFoot ) {}; // fast guy doesn't scuff
    void StopLoopingSounds( void );

    void SoundInit( void );
    void SetIdleSoundState( void );
    void SetAngrySoundState( void );

    void BuildScheduleTestBits( void );

    void BeginNavJump( void );
    void EndNavJump( void );

    bool IsNavJumping( void ) { return m_fIsNavJumping; }
    void OnNavJumpHitApex( void );

    void BeginAttackJump( void );
    void EndAttackJump( void );

    float		MaxYawSpeed( void );

    /*bool SetZombieMode( Zombie_Modes zomMode ) {
        //TGB: ignore if we're jumping/clinging, the whole clinging code is too touchy to have stuff
        //	like this interfere
        if (IsCurSchedule(SCHED_FASTZOMBIE_CEILING_JUMP) ||
            IsCurSchedule(SCHED_FASTZOMBIE_CEILING_CLING))
            return false;

        return BaseClass::SetZombieMode(zomMode);
    }*/

protected:

    static const char *pMoanSounds[];

    // Sound stuff
    float			m_flDistFactor; 
    unsigned char	m_iClimbCount; // counts rungs climbed (for sound)
    bool			m_fIsNavJumping;
    bool			m_fIsAttackJumping;
    bool			m_fHitApex;
    //mutable float	m_flJumpDist;

    bool			m_fHasScreamed;

private:
    float	m_flNextMeleeAttack;
    bool	m_fJustJumped;
    float	m_flJumpStartAltitude;
    float	m_flTimeUpdateSound;

    CSoundPatch	*m_pLayer2; // used for climbing ladders, and when jumping (pre apex)

    bool	m_bClinging;
    Vector	m_vClingJumpStart;
    float	m_flClingLeapStart; //hack, will tell us whether our attack leap comes from ambush

    float	m_flLastClingCheck;
};

IMPLEMENT_SERVERCLASS_ST( CFastZombie, DT_ZM_FastZombie )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( npc_fastzombie, CFastZombie );

BEGIN_DATADESC( CFastZombie )

    DEFINE_FIELD( m_flDistFactor, FIELD_FLOAT ),
    DEFINE_FIELD( m_iClimbCount, FIELD_CHARACTER ),
    DEFINE_FIELD( m_fIsNavJumping, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_fIsAttackJumping, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_fHitApex, FIELD_BOOLEAN ),
    //DEFINE_FIELD( m_flJumpDist, FIELD_FLOAT ),
    DEFINE_FIELD( m_fHasScreamed, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_flNextMeleeAttack, FIELD_TIME ),
    DEFINE_FIELD( m_fJustJumped, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_flJumpStartAltitude, FIELD_FLOAT ),
    DEFINE_FIELD( m_flTimeUpdateSound, FIELD_TIME ),

    // Function Pointers
    DEFINE_ENTITYFUNC( LeapAttackTouch ),
    DEFINE_ENTITYFUNC( ClimbTouch ),
//	DEFINE_SOUNDPATCH( m_pLayer2 ),

END_DATADESC()

const char *CFastZombie::pMoanSounds[] =
{
    "NPC_FastZombie.Moan1",
};

//---------------------------------------------------------
//---------------------------------------------------------
CFastZombie::CFastZombie()
{
    //gEntList.m_BansheeList.AddToTail(this);

    SetZombieClass( ZMCLASS_BANSHEE );
    CZMRules::IncPopCount( GetZombieClass() );
}
//---------------------------------------------------------
//---------------------------------------------------------
CFastZombie::~CFastZombie()
{
    //gEntList.m_BansheeList.FindAndRemove(this);
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CFastZombie::Precache( void )
{
    PrecacheModel("models/zombie/zm_fast.mdl");
    
    PrecacheScriptSound( "NPC_FastZombie.LeapAttack" );
    PrecacheScriptSound( "NPC_FastZombie.FootstepRight" );
    PrecacheScriptSound( "NPC_FastZombie.FootstepLeft" );
    PrecacheScriptSound( "NPC_FastZombie.AttackHit" );
    PrecacheScriptSound( "NPC_FastZombie.AttackMiss" );
    PrecacheScriptSound( "NPC_FastZombie.LeapAttack" );
    PrecacheScriptSound( "NPC_FastZombie.Attack" );
    PrecacheScriptSound( "NPC_FastZombie.Idle" );
    PrecacheScriptSound( "NPC_FastZombie.AlertFar" );
    PrecacheScriptSound( "NPC_FastZombie.AlertNear" );
    PrecacheScriptSound( "NPC_FastZombie.GallopLeft" );
    PrecacheScriptSound( "NPC_FastZombie.GallopRight" );
    PrecacheScriptSound( "NPC_FastZombie.Scream" );
    PrecacheScriptSound( "NPC_FastZombie.RangeAttack" );
    PrecacheScriptSound( "NPC_FastZombie.Frenzy" );
    PrecacheScriptSound( "NPC_FastZombie.NoSound" );
    PrecacheScriptSound( "NPC_FastZombie.Die" );

    PrecacheScriptSound( "NPC_FastZombie.Gurgle" );

    PrecacheScriptSound( "NPC_FastZombie.Moan1" );

    BaseClass::Precache();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CFastZombie::OnScheduleChange( void )
{
    if ( m_flNextMeleeAttack > gpGlobals->curtime + 1 )
    {
        // Allow melee attacks again.
        m_flNextMeleeAttack = gpGlobals->curtime + 0.5;
    }

    BaseClass::OnScheduleChange();
}

//---------------------------------------------------------
//---------------------------------------------------------
int CFastZombie::SelectSchedule ( void )
{
/*	if ( HasCondition( COND_ZOMBIE_RELEASECRAB ) )
    {
        // Death waits for no man. Or zombie. Or something.
        return SCHED_ZOMBIE_RELEASECRAB;
    }
*/

    if ( HasCondition( COND_FASTZOMBIE_CLIMB_TOUCH ) )
    {
        //return SCHED_FASTZOMBIE_UNSTICK_JUMP;
    }

    switch ( m_NPCState )
    {
    case NPC_STATE_COMBAT:
        if ( HasCondition( COND_LOST_ENEMY ) || ( HasCondition( COND_ENEMY_UNREACHABLE ) && MustCloseToAttack() ) )
        {
            // Set state to alert and recurse!
            SetState( NPC_STATE_ALERT );
            return SelectSchedule();
        }
        break;

    case NPC_STATE_ALERT:
        if ( HasCondition( COND_LOST_ENEMY ) || ( HasCondition( COND_ENEMY_UNREACHABLE ) && MustCloseToAttack() ) )
        {
            ClearCondition( COND_LOST_ENEMY );
            ClearCondition( COND_ENEMY_UNREACHABLE );
            SetEnemy( NULL );

#ifdef DEBUG_ZOMBIES
            DevMsg("Wandering\n");
#endif

            // Just lost track of our enemy. 
            // Wander around a bit so we don't look like a dingus.
            return SCHED_ZOMBIE_WANDER_MEDIUM;
        }
        break;
    }

    return BaseClass::SelectSchedule();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFastZombie::PrescheduleThink( void )
{
    BaseClass::PrescheduleThink();

    /*
    if( GetGroundEntity() && GetGroundEntity()->Classify() == CLASS_HEADCRAB )
    {
        // Kill!
        CTakeDamageInfo info;
        info.SetDamage( GetGroundEntity()->GetHealth() );
        info.SetAttacker( this );
        info.SetInflictor( this );
        info.SetDamageType( DMG_GENERIC );
        GetGroundEntity()->TakeDamage( info );
    }
    */

/* 	if( m_pMoanSound && gpGlobals->curtime > m_flTimeUpdateSound )
    {
        // Manage the snorting sound, pitch up for closer.
        float flDistNoBBox;
        if( GetEnemy() && m_NPCState == NPC_STATE_COMBAT )
        {
            flDistNoBBox = ( GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter() ).Length();
            flDistNoBBox -= WorldAlignSize().x;
        }
        else
        {
            // Calm down!
            flDistNoBBox = FASTZOMBIE_EXCITE_DIST;
            m_flTimeUpdateSound += 1.0;
        }
        if( flDistNoBBox >= FASTZOMBIE_EXCITE_DIST && m_flDistFactor != 1.0 )
        {
            // Go back to normal pitch.
            m_flDistFactor = 1.0;
            ENVELOPE_CONTROLLER.SoundChangePitch( m_pMoanSound, FASTZOMBIE_IDLE_PITCH, FASTZOMBIE_SOUND_UPDATE_FREQ );
        }
        else if( flDistNoBBox < FASTZOMBIE_EXCITE_DIST )
        {
            // Zombie is close! Recalculate pitch.
            int iPitch;
            m_flDistFactor = min( 1.0, 1 - flDistNoBBox / FASTZOMBIE_EXCITE_DIST ); 
            iPitch = FASTZOMBIE_MIN_PITCH + ( ( FASTZOMBIE_MAX_PITCH - FASTZOMBIE_MIN_PITCH ) * m_flDistFactor); 
            ENVELOPE_CONTROLLER.SoundChangePitch( m_pMoanSound, iPitch, FASTZOMBIE_SOUND_UPDATE_FREQ );
        }
        m_flTimeUpdateSound = gpGlobals->curtime + FASTZOMBIE_SOUND_UPDATE_FREQ;
    }
*/
    // Crudely detect the apex of our jump
    if( IsNavJumping() && !m_fHitApex && GetAbsVelocity().z <= 0.0 )
    {
        OnNavJumpHitApex();
    }


    if( IsCurSchedule(SCHED_FASTZOMBIE_RANGE_ATTACK1, false) )
    {
        // Think more frequently when flying quickly through the 
        // air, to update the server's location more often.
        SetNextThink(gpGlobals->curtime);
    }
}


//-----------------------------------------------------------------------------
// Purpose: Startup all of the sound patches that the fast zombie uses.
//
//
//-----------------------------------------------------------------------------
void CFastZombie::SoundInit( void )
{//LAWYER:  Cut back on the looping, network intensive sounds
    
/*	if( !m_pMoanSound )
    {
        // !!!HACKHACK - kickstart the moan sound. (sjb)
        MoanSound( envFastZombieMoanVolume, ARRAYSIZE( envFastZombieMoanVolume ) );
        // Clear the commands that the base class gave the moaning sound channel.
        ENVELOPE_CONTROLLER.CommandClear( m_pMoanSound );
    }
    CPASAttenuationFilter filter( this );
    if( !m_pLayer2 )
    {
        // Set up layer2
        m_pLayer2 = ENVELOPE_CONTROLLER.SoundCreate( filter, entindex(), CHAN_VOICE, "NPC_FastZombie.Gurgle", ATTN_NORM );
        // Start silent.
        ENVELOPE_CONTROLLER.Play( m_pLayer2, 0.0, 100 );
    }
*/	
    SetIdleSoundState();
}

//-----------------------------------------------------------------------------
// Purpose: Make the zombie sound calm.
//-----------------------------------------------------------------------------
void CFastZombie::SetIdleSoundState( void )
{ //LAWYER:  This should really be done client-side, it eats so much bandwidth
    // Main looping sound
/*	if ( m_pMoanSound )
    {
        ENVELOPE_CONTROLLER.SoundChangePitch( m_pMoanSound, FASTZOMBIE_IDLE_PITCH, 1.0 );
        ENVELOPE_CONTROLLER.SoundChangeVolume( m_pMoanSound, 0.75, 1.0 );
    }
    // Second Layer
    if ( m_pLayer2 )
    {
        ENVELOPE_CONTROLLER.SoundChangePitch( m_pLayer2, 100, 1.0 );
        ENVELOPE_CONTROLLER.SoundChangeVolume( m_pLayer2, 0.0, 1.0 );
    }*/
}


//-----------------------------------------------------------------------------
// Purpose: Make the zombie sound pizzled
//-----------------------------------------------------------------------------
void CFastZombie::SetAngrySoundState( void )
{
    /*
    if (( !m_pMoanSound ) || ( !m_pLayer2 ))
    {
        return;
    }
    */
    EmitSound( "NPC_FastZombie.LeapAttack" );

/*	// Main looping sound
    ENVELOPE_CONTROLLER.SoundChangePitch( m_pMoanSound, FASTZOMBIE_MIN_PITCH, 0.5 );
    ENVELOPE_CONTROLLER.SoundChangeVolume( m_pMoanSound, 1.0, 0.5 );
    // Second Layer
    ENVELOPE_CONTROLLER.SoundChangePitch( m_pLayer2, 100, 1.0 );
    ENVELOPE_CONTROLLER.SoundChangeVolume( m_pLayer2, 0.0, 1.0 );*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CFastZombie::Spawn( void )
{
    Precache();

    m_bClinging = false;
    m_vClingJumpStart = Vector(0, 0, 0);
    m_flLastClingCheck = 0;
    m_flClingLeapStart = 0;

    m_fJustJumped = false;


    SetBloodColor( BLOOD_COLOR_RED );
    m_iHealth			= zm_sk_banshee_health.GetInt();
    m_flFieldOfView		= 0.1f;

    CapabilitiesClear();
    if (sv_climbing.GetBool() == 1)
    {
        CapabilitiesAdd( bits_CAP_MOVE_JUMP | bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_MOVE_CLIMB);
    }
    else
    {
        CapabilitiesAdd( bits_CAP_MOVE_JUMP | bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK1 /*| bits_CAP_MOVE_CLIMB*/);
    }
    
    m_flNextAttack = gpGlobals->curtime;

    m_pLayer2 = NULL;
    m_iClimbCount = 0;

    EndNavJump();

    m_flDistFactor = 1.0;

    //SetClientSideAnimation(false);

    BaseClass::Spawn();

    //TGB: by default fastzombies have too large a bbox, being hunched over, so correcting that here
    Vector maxs = CollisionProp()->OBBMaxs();
    maxs.z -= 20.0f;
    UTIL_SetSize(this, CollisionProp()->OBBMins(), maxs);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFastZombie::PostNPCInit( void )
{
    SoundInit();

    m_flTimeUpdateSound = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the classname (ie "npc_headcrab") to spawn when our headcrab bails.
//-----------------------------------------------------------------------------
/*const char *CFastZombie::GetHeadcrabClassname( void )
{
    return "npc_headcrab_fast";
}
const char *CFastZombie::GetHeadcrabModel( void )
{
    return "models/headcrab.mdl";
}
*/


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CFastZombie::MaxYawSpeed( void )
{
    switch( GetActivity() )
    {
    case ACT_TURN_LEFT:
    case ACT_TURN_RIGHT:
        return 120;
        break;

    case ACT_WALK:	//TGB: == run now
    case ACT_RUN:
        return 160;
        break;

    case ACT_IDLE:
        return 25;
        break;
        
    default:
        return 20;
        break;
    }
}


//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CFastZombie::SetZombieModel( void )
{
    SetModel( "models/zombie/zm_fast.mdl" );
}

int CFastZombie::MeleeAttack1Conditions( float flDot, float flDist )
{
    if( !(GetFlags() & FL_ONGROUND) )
    {
        // Have to be on the ground!
        return COND_NONE;
    }

    return BaseClass::MeleeAttack1Conditions( flDot, flDist );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a moan sound for this class of zombie.
//-----------------------------------------------------------------------------
const char *CFastZombie::GetMoanSound( int nSound )
{
    return pMoanSounds[ nSound % ARRAYSIZE( pMoanSounds ) ];
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a footstep
//-----------------------------------------------------------------------------
void CFastZombie::FootstepSound( bool fRightFoot )
{
    if( fRightFoot )
    {
        EmitSound( "NPC_FastZombie.FootstepRight" );
    }
    else
    {
        EmitSound( "NPC_FastZombie.FootstepLeft" );
    }
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack hit sound
//-----------------------------------------------------------------------------
void CFastZombie::AttackHitSound( void )
{
    EmitSound( "NPC_FastZombie.AttackHit" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack miss sound
//-----------------------------------------------------------------------------
void CFastZombie::AttackMissSound( void )
{
    // Play a random attack miss sound
    EmitSound( "NPC_FastZombie.AttackMiss" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack sound.
//-----------------------------------------------------------------------------
void CFastZombie::LeapAttackSound( void )
{
    EmitSound( "NPC_FastZombie.LeapAttack" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack sound.
//-----------------------------------------------------------------------------
void CFastZombie::AttackSound( void )
{
    EmitSound( "NPC_FastZombie.Attack" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random idle sound.
//-----------------------------------------------------------------------------
void CFastZombie::IdleSound( void )
{
    EmitSound( "NPC_FastZombie.Idle" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random pain sound.
//-----------------------------------------------------------------------------
void CFastZombie::PainSound( const CTakeDamageInfo &info )
{
    
/*	if ( m_pLayer2 )
        ENVELOPE_CONTROLLER.SoundPlayEnvelope( m_pLayer2, SOUNDCTRL_CHANGE_VOLUME, envFastZombieVolumePain, ARRAYSIZE(envFastZombieVolumePain) );
    if ( m_pMoanSound )
        ENVELOPE_CONTROLLER.SoundPlayEnvelope( m_pMoanSound, SOUNDCTRL_CHANGE_VOLUME, envFastZombieInverseVolumePain, ARRAYSIZE(envFastZombieInverseVolumePain) );*/
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFastZombie::DeathSound( const CTakeDamageInfo &info ) 
{
    EmitSound( "NPC_FastZombie.Die" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random alert sound.
//-----------------------------------------------------------------------------
void CFastZombie::AlertSound( void )
{
    CBaseEntity *pPlayer = AI_GetSinglePlayer();

    if( pPlayer )
    {
        // Measure how far the player is, and play the appropriate type of alert sound. 
        // Doesn't matter if I'm getting mad at a different character, the player is the
        // one that hears the sound.
        float flDist;

        flDist = ( GetAbsOrigin() - pPlayer->GetAbsOrigin() ).Length();

        if( flDist > 512 )
        {
            EmitSound( "NPC_FastZombie.AlertFar" );
        }
        else
        {
            EmitSound( "NPC_FastZombie.AlertNear" );
        }
    }

}

//-----------------------------------------------------------------------------
// Purpose: See if I can make my leaping attack!!
//
//
//-----------------------------------------------------------------------------
int CFastZombie::RangeAttack1Conditions( float flDot, float flDist )
{
#define FASTZOMBIE_MINLEAP			200
#define FASTZOMBIE_MAXLEAP			300

    if (GetEnemy() == NULL)
    {
        return( COND_NONE );
    }

    if( !(GetFlags() & FL_ONGROUND) )
    {
        return COND_NONE;
    }

    if( gpGlobals->curtime < m_flNextAttack )
    {
        return( COND_NONE );
    }

    // make sure the enemy isn't on a roof and I'm in the streets (Ravenholm)
    float flZDist;
    flZDist = fabs( GetEnemy()->GetLocalOrigin().z - GetLocalOrigin().z );
    if( flZDist > 128 )
    {
        return COND_TOO_FAR_TO_ATTACK;
    }

    if( flDist > FASTZOMBIE_MAXLEAP )
    {
        return COND_TOO_FAR_TO_ATTACK;
    }

    if( flDist < FASTZOMBIE_MINLEAP )
    {
        return COND_NONE;
    }

    if (flDot < 0.8) 
    {
        return COND_NONE;
    }

    if ( !IsMoving() )
    {
        // I Have to be running!!!
        return COND_NONE;
    }

    // Don't jump at the player unless he's facing me.
    // This allows the player to get away if he turns and sprints
/*	CBasePlayer *pPlayer = static_cast<CBasePlayer*>( GetEnemy() );
    if( pPlayer )
    {
        // If the enemy is a player, don't attack from behind!
        if( !pPlayer->FInViewCone( this ) )
        {
            return COND_NONE;
        }
    }
*/
    // Drumroll please!
    // The final check! Is the path from my position to halfway between me
    // and the player clear?
    trace_t tr;
    Vector vecDirToEnemy;

    vecDirToEnemy = GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter();
    Vector vecHullMin( -16, -16, -16 );
    Vector vecHullMax( 16, 16, 16 );

    // only check half the distance. (the first part of the jump)
    vecDirToEnemy = vecDirToEnemy * 0.5;

    AI_TraceHull( WorldSpaceCenter(), WorldSpaceCenter() + vecDirToEnemy, vecHullMin, vecHullMax, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

    if( tr.fraction != 1.0 )
    {
        // There's some sort of obstacle pretty much right in front of me.
        return COND_NONE;
    }

    return COND_CAN_RANGE_ATTACK1;
}


//-----------------------------------------------------------------------------
// Purpose:
//
//
//-----------------------------------------------------------------------------
void CFastZombie::HandleAnimEvent( animevent_t *pEvent )
{
    if ( pEvent->event == AE_FASTZOMBIE_CLIMB_LEFT || pEvent->event == AE_FASTZOMBIE_CLIMB_RIGHT )
    {
        if( ++m_iClimbCount % 3 == 0 )
        {
/*			ENVELOPE_CONTROLLER.SoundChangePitch( m_pLayer2, random->RandomFloat( 100, 150 ), 0.0 );
            ENVELOPE_CONTROLLER.SoundPlayEnvelope( m_pLayer2, SOUNDCTRL_CHANGE_VOLUME, envFastZombieVolumeClimb, ARRAYSIZE(envFastZombieVolumeClimb) );*/
        }

        return;
    }

    if ( pEvent->event == AE_FASTZOMBIE_LEAP )
    {
        LeapAttack();
        return;
    }
    
    if ( pEvent->event == AE_FASTZOMBIE_GALLOP_LEFT )
    {
        EmitSound( "NPC_FastZombie.GallopLeft" );
        return;
    }

    if ( pEvent->event == AE_FASTZOMBIE_GALLOP_RIGHT )
    {
        EmitSound( "NPC_FastZombie.GallopRight" );
        return;
    }
    
    if (pEvent->event == AE_ZOMBIE_ATTACK_RIGHT
    ||  pEvent->event == AE_ZOMBIE_ATTACK_LEFT)
    {
        Vector right;
        AngleVectors( GetLocalAngles(), NULL, &right, NULL );
        right = right * -50;
        QAngle viewpunch( -3, -5, -3 );

        ClawAttack( GetClawAttackRange(), zm_sk_banshee_dmg_claw.GetInt(), viewpunch, right,
            pEvent->event == AE_ZOMBIE_ATTACK_RIGHT ? ZOMBIE_BLOOD_RIGHT_HAND : ZOMBIE_BLOOD_LEFT_HAND );
        return;
    }

    BaseClass::HandleAnimEvent( pEvent );
}


//-----------------------------------------------------------------------------
// Purpose: Jump at the enemy!! (stole this from the headcrab)
//
//
//-----------------------------------------------------------------------------
void CFastZombie::LeapAttack( void )
{
    //TGB: if we were already in the air when starting this jump, chances are we need to jump down-
    //	-ward, which needs to be taken into account in the jump code later.
    bool in_air = (GetGroundEntity() == NULL);

    SetGroundEntity( NULL );

    BeginAttackJump();

    LeapAttackSound();

    //
    // Take him off ground so engine doesn't instantly reset FL_ONGROUND.
    //
    UTIL_SetOrigin( this, GetLocalOrigin() + Vector( 0 , 0 , 1 ));

    Vector vecJumpDir;
    CBaseEntity *pEnemy = GetEnemy();

    if ( pEnemy )
    {
        Vector vecEnemyPos = pEnemy->WorldSpaceCenter();

        float gravity = sv_gravity.GetFloat();
        if ( gravity <= 1 )
        {
            gravity = 1;
        }

        //
        // How fast does the zombie need to travel to reach my enemy's eyes given gravity?
        //
        float height = ( vecEnemyPos.z - GetAbsOrigin().z );

        if ( height < 16 )
        {
            height = 16;
        }
        else if ( height > 120 )
        {
            height = 120;
        }
        float speed = sqrt( 2 * gravity * height );
        float time = speed / gravity;

        //
        // Scale the sideways velocity to get there at the right time
        //
        vecJumpDir = vecEnemyPos - GetAbsOrigin();
        vecJumpDir = vecJumpDir / time;

        //
        // Speed to offset gravity at the desired height.
        //

        //TGB: if we were already in the air, we want to jump down
        if (in_air)
        {
            //TGB: leave the (strongly downward) direction alone, but do add a bit of under/overshoot
            //we don't want the lunge to be super-accurate and guarantee damage
            vecJumpDir.z += random->RandomFloat(-32, 32);
        }
        //else we just want the bit of lift we calculated earlier
        else
            vecJumpDir.z = speed;

        //
        // Don't jump too far/fast.
        //
        //TGB: lowered slightly, was: 1000
#define CLAMP 900.0
        float distance = vecJumpDir.Length();
        if ( distance > CLAMP )
        {
            vecJumpDir = vecJumpDir * ( CLAMP / distance );
        }

        // try speeding up a bit.
        SetAbsVelocity( vecJumpDir );
        m_flNextAttack = gpGlobals->curtime + 2;
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFastZombie::StartTask( const Task_t *pTask )
{
    //TGB: apparently there is no method that is called when a schedule is forcibly replaced
    //	so we have to make sure to uncling if we were when doing something new
    if ( m_bClinging && pTask->iTask != TASK_FASTZOMBIE_CLING_TO_CEILING )
    {
        CeilingDetach();

        //push down a bit
        ApplyAbsVelocityImpulse( Vector(0, 0, -100) );
    }


    switch( pTask->iTask )
    {
    case TASK_FASTZOMBIE_VERIFY_ATTACK:
        // Simply ensure that the zombie still has a valid melee attack
        if( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
        {
            TaskComplete();
        }
        else
        {
            TaskFail("");
        }
        break;

    case TASK_FASTZOMBIE_JUMP_BACK:
        {
            SetActivity( ACT_IDLE );

            SetGroundEntity( NULL );

            BeginAttackJump();

            Vector forward;
            AngleVectors( GetLocalAngles(), &forward );

            //
            // Take him off ground so engine doesn't instantly reset FL_ONGROUND.
            //
            UTIL_SetOrigin( this, GetLocalOrigin() + Vector( 0 , 0 , 1 ));

            ApplyAbsVelocityImpulse( forward * -200 + Vector( 0, 0, 200 ) );
        }
        break;

    case TASK_FASTZOMBIE_UNSTICK_JUMP:
        {
            SetGroundEntity( NULL );

            // Call begin attack jump. A little bit later if we fail to pathfind, we check
            // this value to see if we just jumped. If so, we assume we've jumped 
            // to someplace that's not pathing friendly, and so must jump again to get out.
            BeginAttackJump();

            //
            // Take him off ground so engine doesn't instantly reset FL_ONGROUND.
            //
            UTIL_SetOrigin( this, GetLocalOrigin() + Vector( 0 , 0 , 1 ));

            CBaseEntity *pEnemy = GetEnemy();
            Vector vecJumpDir;

            if ( GetActivity() == ACT_CLIMB_UP || GetActivity() == ACT_CLIMB_DOWN )
            {
                // Jump off the pipe backwards!
                Vector forward;

                GetVectors( &forward, NULL, NULL );

                ApplyAbsVelocityImpulse( forward * -200 );
            }
            else if( pEnemy )
            {
                vecJumpDir = pEnemy->GetLocalOrigin() - GetLocalOrigin();
                VectorNormalize( vecJumpDir );
                vecJumpDir.z = 0;

                ApplyAbsVelocityImpulse( vecJumpDir * 300 + Vector( 0, 0, 200 ) );
            }
            else
            {
                DevMsg("UNHANDLED CASE! Stuck Fast Zombie with no enemy!\n");
            }
        }
        break;

    case TASK_WAIT_FOR_MOVEMENT:
        // If we're waiting for movement, that means that pathfinding succeeded, and
        // we're about to be moving. So we aren't stuck. So clear this flag. 
        m_fJustJumped = false;

        BaseClass::StartTask( pTask );
        break;

    case TASK_FACE_ENEMY:
        {
            // We don't use the base class implementation of this, because GetTurnActivity
            // stomps our landing scrabble animations (sjb)
            Vector flEnemyLKP = GetEnemyLKP();
            GetMotor()->SetIdealYawToTarget( flEnemyLKP );
        }
        break;

    case TASK_FASTZOMBIE_LAND_RECOVER:
        {
            // Set the ideal yaw
            Vector flEnemyLKP = GetEnemyLKP();
            GetMotor()->SetIdealYawToTarget( flEnemyLKP );

            // figure out which way to turn.
            float flDeltaYaw = GetMotor()->DeltaIdealYaw();

            if( flDeltaYaw < 0 )
            {
                SetIdealActivity( (Activity)ACT_FASTZOMBIE_LAND_RIGHT );
            }
            else
            {
                SetIdealActivity( (Activity)ACT_FASTZOMBIE_LAND_LEFT );
            }


            TaskComplete();
        }
        break;

    case TASK_RANGE_ATTACK1:

        // Make melee attacks impossible until we land!
        m_flNextMeleeAttack = gpGlobals->curtime + 2;

        SetTouch( &CFastZombie::LeapAttackTouch );
        break;

    case TASK_FASTZOMBIE_DO_ATTACK:
        SetActivity( (Activity)ACT_FASTZOMBIE_LEAP_SOAR );
        break;

    case TASK_FASTZOMBIE_CHECK_CEILING:
        //TGB: first check if the ceiling is nice
        {
            trace_t tr;
            const Vector upwards = Vector(0, 0, FASTZOMBIE_CLING_MAXHEIGHT);
            UTIL_TraceEntity(this, GetAbsOrigin(), GetAbsOrigin() + upwards, MASK_SOLID, &tr);

            //valid surface? that is, a brush that is not the sky
            if (tr.fraction != 1.0f && tr.DidHitWorld() && (tr.surface.flags & SURF_SKY) == false)
            {
                //is it flat enough?
                if (IsCeilingFlat(tr.plane.normal))
                {
                    // this ceiling gets a pass
                    TaskComplete();
                    break;
                }
            }

            //else fail
            //CBasePlayer *pZM = CBasePlayer::GetZM();
            //if (pZM)
            //    ClientPrint(pZM, HUD_PRINTTALK, "Banshee was unable to find a solid ceiling within range above it!\n");

            TaskFail("");
        }
        break;

    case TASK_FASTZOMBIE_JUMP_TO_CEILING:
        //TGB: if the ceiling is nice, perform jump

        // set up handler for when we reach the roof
        SetTouch( &CFastZombie::CeilingJumpTouch );

        // perform "jump"
        ApplyAbsVelocityImpulse( Vector(0, 0, FASTZOMBIE_CLING_JUMPSPEED) );

        m_vClingJumpStart = GetAbsOrigin();

        TaskComplete();
        break;

    case TASK_FASTZOMBIE_CLING_TO_CEILING:
        //stick to it
        //AddFlag( FL_FLY );
        SetMoveType(MOVETYPE_NONE);
        //SetSolid( SOLID_BBOX );
        //SetGravity( 0.0 );
        
        //TGB: handled in the touch func
        //SetGroundEntity( NULL ); //otherwise it will probably assume to be on the floor and not fall when done

        SetIdealActivity(ACT_HOVER);

        m_bClinging = true;


        break;

    default:
        BaseClass::StartTask( pTask );
        break;
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFastZombie::RunTask( const Task_t *pTask )
{
    switch( pTask->iTask )
    {
    case TASK_FASTZOMBIE_JUMP_BACK:
    case TASK_FASTZOMBIE_UNSTICK_JUMP:
        if( GetFlags() & FL_ONGROUND )
        {
            TaskComplete();
        }
        break;

    case TASK_RANGE_ATTACK1:
        if( GetFlags() & FL_ONGROUND )
        {
            // All done when you touch the ground.
            TaskComplete();

            // Allow melee attacks again.
            m_flNextMeleeAttack = gpGlobals->curtime + 0.5;
            return;
        }
        break;

    case TASK_FASTZOMBIE_CLING_TO_CEILING:
        if (HasCondition(COND_LIGHT_DAMAGE) || HasCondition(COND_RECEIVED_ORDERS))
        {
            CeilingDetach();
            
            //push down a bit
            ApplyAbsVelocityImpulse( Vector(0, 0, -100) );

            TaskComplete();
            break;
        }


        // TODO: check for a ceiling that moves from underneath us when clinging [16-10-2008]


        //TGB: RunTask is called every 0.2 sec or so (prolly tickrate related), we don't need to check that often
        if (m_flLastClingCheck < gpGlobals->curtime)
        {
            //DevMsg("FZ: checking for enemy  %f\n", gpGlobals->curtime);

            //TGB: check if there is anyone walking underneath us
            CBaseEntity *nearest = GetClingAmbushTarget();

            // found someone underneath us
            if (nearest != NULL)
            {
            //	DevMsg("FZ: found an enemy, letting go..\n");

                // go back to gravity-manipulated movetype
                CeilingDetach();

                // force this guy to be our enemy for when we leap
                SetEnemy( nearest );

                //LeapAttack();
                //SetActivity(TASK_RANGE_ATTACK1);

                //TGB: rather than forcibly switching to a new schedule (which takes time), transition to next task
                TaskComplete();
                //next task will be the attack
                m_flClingLeapStart = gpGlobals->curtime;
            }

            /* TGB: this won't be necessary until we support clinging to ents, which should only be done for specific cases to avoid headache
            //TGB: also do a quick check to see if our ceiling is still around
            trace_t tr;
            const Vector upwards = Vector(0, 0, 5);
            UTIL_TraceEntity(this, GetAbsOrigin(), GetAbsOrigin() + upwards, MASK_SOLID, &tr);
            //if there's nothing there, or there's something that's not what we initially clinged to, detach
            if (tr.fraction == 1.0f || (tr.m_pEnt != NULL && tr.m_pEnt != GetGroundEntity()))
            {
                CeilingDetach();
            }
            */

            m_flLastClingCheck = gpGlobals->curtime + 1.0f; //1s delay
        }
        break;
    default:
        BaseClass::RunTask( pTask );
        break;
    }
}


//---------------------------------------------------------
//---------------------------------------------------------
int CFastZombie::TranslateSchedule( int scheduleType )
{
    switch( scheduleType )
    {
    case SCHED_RANGE_ATTACK1:
        {
            // Scream right now, cause in half a second, we're gonna jump!!
    
            if( !m_fHasScreamed )
            {
                // Only play that over-the-top attack scream once per combat state.
                EmitSound( "NPC_FastZombie.Scream" );
                m_fHasScreamed = true;
            }
            else
            {
                EmitSound( "NPC_FastZombie.RangeAttack" );
            }

            return SCHED_FASTZOMBIE_RANGE_ATTACK1;
        }
        break;

    case SCHED_MELEE_ATTACK1:
        return SCHED_FASTZOMBIE_MELEE_ATTACK1;
        break;

    case SCHED_FASTZOMBIE_UNSTICK_JUMP:
        if ( GetActivity() == ACT_CLIMB_UP || GetActivity() == ACT_CLIMB_DOWN || GetActivity() == ACT_CLIMB_DISMOUNT )
        {
            return SCHED_FASTZOMBIE_CLIMBING_UNSTICK_JUMP;
        }
        else
        {
            return SCHED_FASTZOMBIE_UNSTICK_JUMP;
        }
        break;

    case SCHED_FASTZOMBIE_CEILING_CLING:
        return SCHED_FASTZOMBIE_CEILING_CLING;
        break;

    case SCHED_FASTZOMBIE_CEILING_JUMP:
        return SCHED_FASTZOMBIE_CEILING_JUMP;
        break;

    default:
        return BaseClass::TranslateSchedule( scheduleType );
        break;
    }
}

//---------------------------------------------------------
//---------------------------------------------------------
Activity CFastZombie::NPC_TranslateActivity( Activity baseAct )
{
    if ( baseAct == ACT_CLIMB_DOWN )
        return ACT_CLIMB_UP;

    return BaseClass::NPC_TranslateActivity( baseAct );
}

//---------------------------------------------------------
//---------------------------------------------------------
void CFastZombie::LeapAttackTouch( CBaseEntity *pOther )
{
    if ( !pOther->IsSolid() )
    {
        // Touching a trigger or something.
        return;
    }

    // ZMRCHANGE: Don't bother with weapons/ammo.
    if ( pOther->GetCollisionGroup() == COLLISION_GROUP_WEAPON )
        return;


    // Stop the zombie and knock the player back
    Vector vecNewVelocity( 0, 0, GetAbsVelocity().z );
    SetAbsVelocity( vecNewVelocity );

    Vector forward;
    AngleVectors( GetLocalAngles(), &forward );
    QAngle qaPunch( 15, random->RandomInt(-5,5), random->RandomInt(-5,5) );
    
    float damage = zm_sk_banshee_dmg_leap.GetFloat();

    //if we were clinging moments ago, give a damage bonus
    if (m_flClingLeapStart >= (gpGlobals->curtime - 3.0f))
    {
        damage *= 1.4f; //attack from ceiling ambush = 40% bonus (5 dmg -> 7 dmg)

        m_flClingLeapStart = 0; //to make sure it only happens once
    }	
    
    Vector punchvel = forward * 500;

    ClawAttack( GetClawAttackRange(), (int)damage, qaPunch, punchvel, ZOMBIE_BLOOD_BOTH_HANDS );

    SetTouch( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Lets us know if we touch the player while we're climbing.
//-----------------------------------------------------------------------------
void CFastZombie::ClimbTouch( CBaseEntity *pOther )
{
    //TGB: attempt to extend this to NPCs as well, to prevent fasties from getting stuck together
    if ( pOther->IsPlayer() || pOther->IsNPC() )
    {
        // If I hit the player, shove him aside.
        Vector vecDir = pOther->WorldSpaceCenter() - WorldSpaceCenter();
        vecDir.z = 0.0; // planar
        VectorNormalize( vecDir );
        pOther->VelocityPunch( vecDir * 200 );

        if ( GetActivity() != ACT_CLIMB_DISMOUNT || 
             ( pOther->GetGroundEntity() == NULL &&
               GetNavigator()->IsGoalActive() &&
               pOther->GetAbsOrigin().z - GetNavigator()->GetCurWaypointPos().z < -1.0 ) )
        {
            SetCondition( COND_FASTZOMBIE_CLIMB_TOUCH );
        }

        SetTouch( NULL );
    }
    else if ( dynamic_cast<CPhysicsProp *>(pOther) )
    {
        NPCPhysics_CreateSolver( this, pOther, true, 5.0 );
    }
}


void CFastZombie::CeilingJumpTouch(CBaseEntity *pOther )
{
    if ( !pOther->IsSolid() )
    {
        // Touching a trigger or something.
        return;
    }

    // Stop the zombie
    SetAbsVelocity( Vector(0, 0, 0) );

    SetTouch( NULL );

    trace_t	tr;
    tr = BaseClass::GetTouchTrace();

    //TODO: move check into separate func?
    if (tr.DidHitWorld() && (tr.surface.flags & SURF_SKY) == false)
    {
        //is it flat enough?
        if (IsCeilingFlat(tr.plane.normal))
        {
            //const Vector newpos = GetAbsOrigin() + Vector(0, 0, 30);
            //Teleport( &newpos, NULL, NULL);
            SetMoveType(MOVETYPE_NONE);
            SetSchedule(SCHED_FASTZOMBIE_CEILING_CLING);

            SetGroundEntity(tr.m_pEnt);
        }
        else
        {
            //recover from a bad landing of some sort, either on a sloped ceiling or the ground
            SetMoveType(MOVETYPE_STEP);
            SetIdealActivity((Activity)ACT_IDLE);
            SetCondition(COND_TASK_FAILED);

            SetGroundEntity(NULL);
        }
    }
    else
    {
        DevMsg("Banshee hit something bad while jumping!");
        SetCondition(COND_TASK_FAILED);
    }
}

//-----------------------------------------------------------------------------
// Purpose: Shuts down our looping sounds.
//-----------------------------------------------------------------------------
void CFastZombie::StopLoopingSounds( void )
{
/*	if ( m_pMoanSound )
    {
        ENVELOPE_CONTROLLER.SoundDestroy( m_pMoanSound );
        m_pMoanSound = NULL;
    }
    if ( m_pLayer2 )
    {
        ENVELOPE_CONTROLLER.SoundDestroy( m_pLayer2 );
        m_pLayer2 = NULL;
    }
*/
    BaseClass::StopLoopingSounds();
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if a reasonable jumping distance
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CFastZombie::IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const
{
    // ZMRCHANGE: Increase max drop and use our own dist check to ignore z-axis.
    // Eg. zm_desert_skystation

    const float MAX_JUMP_RISE		= 220.0f;
    const float MAX_JUMP_DISTANCE	= 512.0f;
    const float MAX_JUMP_DROP		= 1024.0f; // 384.0f

    if ( (endPos.z - startPos.z) > MAX_JUMP_RISE + 0.1f ) 
        return false;
    if ( (startPos.z - endPos.z) > MAX_JUMP_DROP + 0.1f ) 
        return false;
    if ( (apex.z - startPos.z) > MAX_JUMP_RISE * 1.25f ) 
        return false;


    // Not used?

    // Hang onto the jump distance. The AI is going to want it.
    //m_flJumpDist = (startPos - endPos).Length();


    if ( (startPos - endPos).Length2D() > MAX_JUMP_DISTANCE + 0.1f ) 
        return false;

    return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool CFastZombie::MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost )
{
    float delta = vecEnd.z - vecStart.z;

    float multiplier = 1;
    if ( moveType == bits_CAP_MOVE_JUMP )
    {
        multiplier = ( delta < 0 ) ? 0.5 : 1.5;
    }
    else if ( moveType == bits_CAP_MOVE_CLIMB )
    {
        multiplier = ( delta > 0 ) ? 0.5 : 4.0;
    }

    *pCost *= multiplier;

    return ( multiplier != 1 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool CFastZombie::ShouldFailNav( bool bMovementFailed )
{
    if ( !BaseClass::ShouldFailNav( bMovementFailed ) )
    {
        DevMsg( 2, "Fast zombie in scripted sequence probably hit bad node configuration at %s\n", VecToString( GetAbsOrigin() ) );
        
        if ( GetNavigator()->GetPath()->CurWaypointNavType() == NAV_JUMP && GetNavigator()->RefindPathToGoal( false ) )
        {
            return false;
        }
        DevMsg( 2, "Fast zombie failed to get to scripted sequence\n" );
    }

    return true;
}


//---------------------------------------------------------
// Purpose: Notifier that lets us know when the fast
//			zombie has hit the apex of a navigational jump.
//---------------------------------------------------------
void CFastZombie::OnNavJumpHitApex( void )
{
    m_fHitApex = true;	// stop subsequent notifications
}

//---------------------------------------------------------
// Purpose: Overridden to detect when the zombie goes into
//			and out of his climb state and his navigation
//			jump state.
//---------------------------------------------------------
void CFastZombie::OnChangeActivity( Activity NewActivity )
{
    if ( NewActivity == ACT_FASTZOMBIE_FRENZY )
    {
        // Scream!!!!
        EmitSound( "NPC_FastZombie.Frenzy" );
        SetPlaybackRate( random->RandomFloat( .9, 1.1 ) );	
    }

    if( NewActivity == ACT_JUMP )
    {
        BeginNavJump();
    }
    else if( GetActivity() == ACT_JUMP )
    {
        EndNavJump();
    }

    if ( NewActivity == ACT_LAND )
    {
        m_flNextAttack = gpGlobals->curtime + 1.0;
    }

    if ( NewActivity == ACT_GLIDE )
    {
        // Started a jump.
        BeginNavJump();
    }
    else if ( GetActivity() == ACT_GLIDE )
    {
        // Landed a jump
        EndNavJump();

/*		if ( m_pMoanSound )
            ENVELOPE_CONTROLLER.SoundChangePitch( m_pMoanSound, FASTZOMBIE_MIN_PITCH, 0.3 );*/
    }

    if ( NewActivity == ACT_CLIMB_UP )
    {
        // Started a climb!
/*		if ( m_pMoanSound )
            ENVELOPE_CONTROLLER.SoundChangeVolume( m_pMoanSound, 0.0, 0.2 );
*/
        SetTouch( &CFastZombie::ClimbTouch );
    }
    else if ( GetActivity() == ACT_CLIMB_DISMOUNT || ( GetActivity() == ACT_CLIMB_UP && NewActivity != ACT_CLIMB_DISMOUNT ) )
    {
        // Ended a climb
/*		if ( m_pMoanSound )
            ENVELOPE_CONTROLLER.SoundChangeVolume( m_pMoanSound, 1.0, 0.2 );
*/
        SetTouch( NULL );
    }

    //TGBTEST
    if (NewActivity == ACT_HOP)
    {
        trace_t tr;
        UTIL_TraceEntity(this, GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, 400), MASK_SOLID, &tr);
        
        if (tr.fraction != 1.0f && tr.DidHitWorld() && (tr.surface.flags & SURF_SKY) == false)
        {
            IsCeilingFlat(tr.plane.normal);
        }
        
    }
    

    BaseClass::OnChangeActivity( NewActivity );
}


//=========================================================
// 
//=========================================================
int CFastZombie::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
    if ( m_fJustJumped )
    {
        // Assume we failed cause we jumped to a bad place.
        m_fJustJumped = false;
        return SCHED_FASTZOMBIE_UNSTICK_JUMP;
    }

    return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//=========================================================
// Purpose: Do some record keeping for jumps made for 
//			navigational purposes (i.e., not attack jumps)
//=========================================================
void CFastZombie::BeginNavJump( void )
{
    m_fIsNavJumping = true;
    m_fHitApex = false;

//	ENVELOPE_CONTROLLER.SoundPlayEnvelope( m_pLayer2, SOUNDCTRL_CHANGE_VOLUME, envFastZombieVolumeJump, ARRAYSIZE(envFastZombieVolumeJump) );
}

//=========================================================
// 
//=========================================================
void CFastZombie::EndNavJump( void )
{
    m_fIsNavJumping = false;
    m_fHitApex = false;
}

//=========================================================
// 
//=========================================================
void CFastZombie::BeginAttackJump( void )
{
    // Set this to true. A little bit later if we fail to pathfind, we check
    // this value to see if we just jumped. If so, we assume we've jumped 
    // to someplace that's not pathing friendly, and so must jump again to get out.
    m_fJustJumped = true;

    m_flJumpStartAltitude = GetLocalOrigin().z;
}

//=========================================================
// 
//=========================================================
void CFastZombie::EndAttackJump( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFastZombie::BuildScheduleTestBits( void )
{
    // Any schedule that makes us climb should break if we touch player
    if ( GetActivity() == ACT_CLIMB_UP || GetActivity() == ACT_CLIMB_DOWN || GetActivity() == ACT_CLIMB_DISMOUNT )
    {
        SetCustomInterruptCondition( COND_FASTZOMBIE_CLIMB_TOUCH );
    }
    else
    {
        ClearCustomInterruptCondition( COND_FASTZOMBIE_CLIMB_TOUCH );
    }
}

//=========================================================
// 
//=========================================================
void CFastZombie::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
    if( NewState == NPC_STATE_COMBAT )
    {
        SetAngrySoundState();
    }
    else if( ( NewState == NPC_STATE_IDLE || NewState == NPC_STATE_ALERT ) ) ///!!!HACKHACK - sjb
    {
        // Set it up so that if the zombie goes into combat state sometime down the road
        // that he'll be able to scream.
        m_fHasScreamed = false;

        SetIdleSoundState();
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFastZombie::Event_Killed( const CTakeDamageInfo &info )
{
    // Shut up my screaming sounds.
    CPASAttenuationFilter filter( this );
    EmitSound( filter, entindex(), "NPC_FastZombie.NoSound" );

    //TGB: was a report about sound persisting, so just to be sure
    StopSound( entindex(), "NPC_FastZombie.LeapAttack");

    BaseClass::Event_Killed( info );
}

//--------------------------------------------------------------
// TGB: helper/encapsulation for checking if a ceiling is flat enough for standing on
//--------------------------------------------------------------
bool CFastZombie::IsCeilingFlat(Vector plane_normal )
{
    const Vector flat = Vector(0, 0, -1);
    const float roofdot = DotProductAbs(plane_normal, flat);

    if (roofdot > 0.95f)
    {
        DevMsg("Banshee: roof dotproduct %f\n", roofdot);
        return true;
    }

    return false;
}

//--------------------------------------------------------------
// TGB: another case of moving code out into a func for tidiness
//--------------------------------------------------------------
CBaseEntity* CFastZombie::GetClingAmbushTarget()
{
    // look around target
    CBaseEntity* pList[32];
    const int count = UTIL_EntitiesInSphere(pList, ARRAYSIZE( pList ), m_vClingJumpStart, FASTZOMBIE_CLING_DETECTRANGE, FL_CLIENT);

    // loop through any finds
    CBaseEntity *nearest = nullptr;
    float nearest_dist = 0;
    for ( int i = 0; i < count; i++ )
    {
        if ( !pList[i] )
            continue;

        if ( pList[i]->GetTeamNumber() != ZMTEAM_HUMAN || !pList[i]->IsAlive() )
            continue;

        if ( !FVisible( pList[i], MASK_SOLID ) )
            continue; //skip out of LOS

        const float current_dist = EnemyDistance(pList[i]);
        if (!nearest || nearest_dist > current_dist)
        {
            nearest = pList[i];
            nearest_dist = current_dist;
        }
    }

    return nearest;
}

//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_fastzombie, CFastZombie )

    DECLARE_ACTIVITY( ACT_FASTZOMBIE_LEAP_SOAR )
    DECLARE_ACTIVITY( ACT_FASTZOMBIE_LEAP_STRIKE )
    DECLARE_ACTIVITY( ACT_FASTZOMBIE_LAND_RIGHT )
    DECLARE_ACTIVITY( ACT_FASTZOMBIE_LAND_LEFT )
    DECLARE_ACTIVITY( ACT_FASTZOMBIE_FRENZY )
    DECLARE_ACTIVITY( ACT_FASTZOMBIE_BIG_SLASH )
    
    DECLARE_TASK( TASK_FASTZOMBIE_DO_ATTACK )
    DECLARE_TASK( TASK_FASTZOMBIE_LAND_RECOVER )
    DECLARE_TASK( TASK_FASTZOMBIE_UNSTICK_JUMP )
    DECLARE_TASK( TASK_FASTZOMBIE_JUMP_BACK )
    DECLARE_TASK( TASK_FASTZOMBIE_VERIFY_ATTACK )

    //tgb
    DECLARE_TASK( TASK_FASTZOMBIE_JUMP_TO_CEILING )
    DECLARE_TASK( TASK_FASTZOMBIE_CLING_TO_CEILING )
    DECLARE_TASK( TASK_FASTZOMBIE_CHECK_CEILING )

    DECLARE_CONDITION( COND_FASTZOMBIE_CLIMB_TOUCH )

    //Adrian: events go here
    DECLARE_ANIMEVENT( AE_FASTZOMBIE_LEAP )
    DECLARE_ANIMEVENT( AE_FASTZOMBIE_GALLOP_LEFT )
    DECLARE_ANIMEVENT( AE_FASTZOMBIE_GALLOP_RIGHT )
    DECLARE_ANIMEVENT( AE_FASTZOMBIE_CLIMB_LEFT )
    DECLARE_ANIMEVENT( AE_FASTZOMBIE_CLIMB_RIGHT )

    //=========================================================
    // 
    //=========================================================
    DEFINE_SCHEDULE
    (
        SCHED_FASTZOMBIE_RANGE_ATTACK1,

        "	Tasks"
        "		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_RANGE_ATTACK1"
        "		TASK_SET_ACTIVITY				ACTIVITY:ACT_FASTZOMBIE_LEAP_STRIKE"
        "		TASK_RANGE_ATTACK1				0"
        "		TASK_WAIT						0.1"
        "		TASK_FASTZOMBIE_LAND_RECOVER	0" // essentially just figure out which way to turn.
        "		TASK_FACE_ENEMY					0"
        "	"
        "	Interrupts"
    )

    //=========================================================
    // I have landed somewhere that's pathfinding-unfriendly
    // just try to jump out.
    //=========================================================
    DEFINE_SCHEDULE
    (
        SCHED_FASTZOMBIE_UNSTICK_JUMP,

        "	Tasks"
        "		TASK_FASTZOMBIE_UNSTICK_JUMP	0"
        "	"
        "	Interrupts"
    )

    //=========================================================
    //=========================================================
    DEFINE_SCHEDULE
    (
        SCHED_FASTZOMBIE_CLIMBING_UNSTICK_JUMP,

        "	Tasks"
        "		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
        "		TASK_FASTZOMBIE_UNSTICK_JUMP	0"
        "	"
        "	Interrupts"
    )

    //--------------------------------------------------------------
    // TGB: test schedule for making fasties cling to ceilings when told to
    //--------------------------------------------------------------
    DEFINE_SCHEDULE
    (
        SCHED_FASTZOMBIE_CEILING_JUMP,

        "	Tasks"
        "		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
        "		TASK_FASTZOMBIE_CHECK_CEILING	0"					//check if valid
        "		TASK_FASTZOMBIE_JUMP_TO_CEILING	0"					//perform jump
        "		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_HOP" //play anim
        "	"
        "	Interrupts"
        "		COND_TASK_FAILED"
        "		COND_RECEIVED_ORDERS"
    )

    DEFINE_SCHEDULE
    (
        SCHED_FASTZOMBIE_CEILING_CLING,

        "	Tasks"
        "		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"
        "		TASK_FASTZOMBIE_CLING_TO_CEILING	99999"
        "		TASK_WAIT							0.2"
        //transition into attack
        "		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_RANGE_ATTACK1"
        "		TASK_SET_ACTIVITY				ACTIVITY:ACT_FASTZOMBIE_LEAP_STRIKE"
        "		TASK_RANGE_ATTACK1				0"
        "		TASK_WAIT						0.1"
        "		TASK_FASTZOMBIE_LAND_RECOVER	0"
        "		TASK_FACE_ENEMY					0"
        "	"
        "	Interrupts"
        "		COND_TASK_FAILED"
        //"		COND_NEW_ENEMY"	
        "		COND_LIGHT_DAMAGE"
        "		COND_RECEIVED_ORDERS"
    )

    //=========================================================
    // > Melee_Attack1
    //=========================================================
    DEFINE_SCHEDULE
    (
        SCHED_FASTZOMBIE_MELEE_ATTACK1,

        "	Tasks"
        "		TASK_STOP_MOVING				0"
        "		TASK_FACE_ENEMY					0"
        "		TASK_MELEE_ATTACK1				0"
        "		TASK_MELEE_ATTACK1				0"
        "		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_FASTZOMBIE_FRENZY"
        "		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY"
        "		TASK_FASTZOMBIE_VERIFY_ATTACK	0"
        "		TASK_PLAY_SEQUENCE_FACE_ENEMY	ACTIVITY:ACT_FASTZOMBIE_BIG_SLASH"
        ""
        "	Interrupts"
        "		COND_NEW_ENEMY"
        "		COND_ENEMY_DEAD"
        "		COND_ENEMY_OCCLUDED"
    );

AI_END_CUSTOM_NPC()
