#pragma once

#include "hl2/npc_BaseZombie.h"


#include "zmr/zmr_player_shared.h"
#include "zmr/zmr_shareddefs.h"
#include "zmr_npclagcomp.h"


/*
    NOTE: You have to remove files from the project:

    npc_zombie.cpp
    npc_PoisonZombie.cpp
*/

enum
{
    SCHED_ZM_GO = LAST_BASE_ZOMBIE_SCHEDULE + 15, // HACK
    SCHED_ZM_FORCED_GO
};

enum
{
    COND_ZM_SEE_ENEMY = LAST_BASE_ZOMBIE_CONDITION + 15, // HACK
};

class CZMBaseZombie : public CNPC_BaseZombie, public CZMNPCLagCompensation
{
public:
    DECLARE_CLASS( CZMBaseZombie, CNPC_BaseZombie )
    DECLARE_SERVERCLASS()
    DECLARE_DATADESC()
    DEFINE_CUSTOM_AI;

    CZMBaseZombie();
    ~CZMBaseZombie();
    

    virtual void Spawn() OVERRIDE;
    virtual void Precache() OVERRIDE;

    virtual int OnTakeDamage_Alive( const CTakeDamageInfo& ) OVERRIDE;
    virtual void HandleAnimEvent( animevent_t* ) OVERRIDE;
    
    virtual bool IsValidEnemy( CBaseEntity* ) OVERRIDE;
    virtual bool IsChopped( const CTakeDamageInfo &info ) OVERRIDE;
    virtual bool CanBecomeLiveTorso() OVERRIDE;
	virtual bool ShouldBecomeTorso( const CTakeDamageInfo &info, float flDamageThreshold ) OVERRIDE;
    virtual HeadcrabRelease_t ShouldReleaseHeadcrab( const CTakeDamageInfo &info, float flDamageThreshold ) OVERRIDE;
    virtual bool CorpseGib( const CTakeDamageInfo &info ) OVERRIDE;
    
    virtual const char *GetLegsModel( void ) OVERRIDE { return ""; };
	virtual const char *GetTorsoModel( void ) OVERRIDE { return ""; };
	virtual const char *GetHeadcrabClassname( void ) OVERRIDE { return ""; };
	virtual const char *GetHeadcrabModel( void ) OVERRIDE { return ""; };

    virtual void SetZombieModel( void ) OVERRIDE;

    virtual bool ShouldGib( const CTakeDamageInfo& ) OVERRIDE;

    
    int GetSwatActivity( void );
    virtual void StartTask( const Task_t* ) OVERRIDE;
    virtual void GatherConditions( void ) OVERRIDE;
    virtual int TranslateSchedule( int ) OVERRIDE;
    virtual int SelectSchedule( void ) OVERRIDE;
    virtual bool OnInsufficientStopDist( AILocalMoveGoal_t*, float, AIMoveResult_t* ) OVERRIDE;

    // By default don't swat objects.
    virtual bool CanSwatPhysicsObjects() OVERRIDE { return false; };
    bool FindNearestPhysicsObject( int iMaxMass );

    // Always classify as a zombie. The AI relationships depend on it.
    virtual Class_T Classify( void ) OVERRIDE { return CLASS_ZOMBIE; };
    Disposition_t IRelationType( CBaseEntity* pTarget ) OVERRIDE { return CAI_BaseNPC::IRelationType( pTarget ); };

    virtual int MeleeAttack1Conditions( float, float ) OVERRIDE;
    virtual bool MustCloseToAttack() OVERRIDE { return true; };

    // Our stuff...
    int GetPopCost() { return CZMBaseZombie::GetPopCost( GetZombieClass() ); };
    int GetCost() { return CZMBaseZombie::GetCost( GetZombieClass() ); };
    ZombieClass_t GetZombieClass() { return m_iZombieClass; };

    
    // Player commanded means the zombie will ignore new enemies for a short period of time.
    virtual void Command( const Vector&, bool bPlayerCommanded = true );
    virtual bool Swat( CBaseEntity*, bool );
    void SwatObject( IPhysicsObject*, Vector& );
    virtual bool TargetEnemy( CBaseEntity* );


    bool CanSpawn( const Vector& );


    // Implemented in zmr_zombiebase_shared
    static bool IsValidClass( ZombieClass_t );
    static ZombieClass_t NameToClass( const char* );
    static const char* ClassToName( ZombieClass_t );
    static int GetPopCost( ZombieClass_t );
    static int GetCost( ZombieClass_t );
    static bool HasEnoughPopToSpawn( ZombieClass_t );
    int GetSelectorIndex();
    CZMPlayer* GetSelector();
    void SetSelector( CZMPlayer* );
    void SetSelector( int );

private:
    ZombieClass_t m_iZombieClass;

    
    float m_flLastCommand;

    bool m_bSwatBreakable;
    
protected:
    inline void SetZombieClass( ZombieClass_t zclass ) { m_iZombieClass = zclass; };


    CNetworkVar( int, m_iSelectorIndex );
    CNetworkVar( float, m_flHealthRatio ); // For humans we can use health/maxhealth
};