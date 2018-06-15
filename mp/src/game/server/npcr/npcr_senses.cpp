#include "cbase.h"
#include "soundent.h"

#include "npcr_nonplayer.h"
#include "npcr_senses.h"



#define VISION_LASTSEEN_GRACE   0.5f

NPCR::CBaseSenses::CBaseSenses( NPCR::CBaseNPC* pNPC ) : NPCR::CEventListener( pNPC, pNPC )
{
}

NPCR::CBaseSenses::~CBaseSenses()
{
}

void NPCR::CBaseSenses::Update()
{
    if ( ShouldUpdateVision() )
        UpdateVision();

    if ( ShouldUpdateHearing() )
        UpdateHearing();
}

bool NPCR::CBaseSenses::ShouldUpdateVision()
{
    return m_NextVisionTimer.IsElapsed();
}

bool NPCR::CBaseSenses::ShouldUpdateHearing()
{
    return m_NextHearingTimer.IsElapsed();
}

void NPCR::CBaseSenses::UpdateHearing()
{
    if ( 1 /*&& !(GetOuter()->HasSpawnFlags(SF_NPC_WAIT_TILL_SEEN))*/ )
    {
        const int iSoundMask = GetSoundMask();
    
        int	iSound = CSoundEnt::ActiveList();
        while ( iSound != SOUNDLIST_EMPTY )
        {
            CSound* pSound = CSoundEnt::SoundPointerForIndex( iSound );

            if ( !pSound )
                break;

            if ( (iSoundMask & pSound->SoundType()) )
            {
                if ( CanHearSound( pSound ) )
                {
                    // the npc cares about this sound, and it's close enough to hear.
                    //pCurrentSound->m_iNextAudible = m_iAudibleList;
                    //m_iAudibleList = iSound;
                    GetNPC()->OnHeardSound( pSound );
                }
            }

            iSound = pSound->NextSound();
        }
    }

    m_NextHearingTimer.Start( 0.1f );
}

int NPCR::CBaseSenses::GetSoundMask() const
{
    return SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_BULLET_IMPACT;
}

bool NPCR::CBaseSenses::CanHearSound( CSound* pSound ) const
{
    if ( pSound->Volume() <= 0.0f )
        return false;


    float flHearingDist = pSound->Volume();
    flHearingDist *= flHearingDist;

    return pSound->GetSoundOrigin().DistToSqr( GetNPC()->GetPosition() ) < flHearingDist;
}

void NPCR::CBaseSenses::UpdateVision()
{
    m_flFovCos = cosf( DEG2RAD( GetFieldOfView() / 2.0f ) );


    for ( int i = m_vVisionEnts.Count() - 1; i >= 0; i-- )
    {
        if ( !CanSee( m_vVisionEnts[i] ) )
        {
            CBaseEntity* pLost = m_vVisionEnts[i]->GetEntity();

            if ( pLost )
                GetNPC()->OnSightLost( m_vVisionEnts[i]->GetEntity() );

            m_vVisionEnts.Remove( i );
        }
    }
    
    CUtlVector<CBaseEntity*> vListEnts;
    FindNewEntities( vListEnts );

    FOR_EACH_VEC( vListEnts, i )
    {
        if ( !CanSee( vListEnts[i]->WorldSpaceCenter() ) )
            continue;

        bool bSeen = false;

        FOR_EACH_VEC( m_vVisionEnts, j )
        {
            if ( vListEnts[i] == m_vVisionEnts[j]->GetEntity() )
            {
                m_vVisionEnts[j]->UpdateLastSeen();
                bSeen = true;
                break;
            }
        }

        if ( !bSeen )
        {
            m_vVisionEnts.AddToTail( new VisionEntity( vListEnts[i] ) );

            GetNPC()->OnSightGained( vListEnts[i] );
        }
    }

    m_NextVisionTimer.Start( 0.1f );
}

void NPCR::CBaseSenses::FindNewEntities( CUtlVector<CBaseEntity*>& vListEnts )
{
    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        CBasePlayer* pPlayer = UTIL_PlayerByIndex( i );
        if ( pPlayer && GetNPC()->IsEnemy( pPlayer ) )
        {
            vListEnts.AddToTail( pPlayer );
        }
    }
}

bool NPCR::CBaseSenses::CanSee( CBaseEntity* pEnt ) const
{
    FOR_EACH_VEC( m_vVisionEnts, i )
    {
        if ( m_vVisionEnts[i]->GetEntity() == pEnt )
            return CanSee( m_vVisionEnts[i] );
    }

    return false;
}

bool NPCR::CBaseSenses::CanSee( VisionEntity* pVision ) const
{
    CBaseEntity* pEnt = pVision->GetEntity();
    if ( !pEnt )
        return false;
    if ( (gpGlobals->curtime - pVision->LastSeen()) > VISION_LASTSEEN_GRACE )
        return true;


    //GetNPC();
    //UTIL_TraceLine(  )

    if ( CanSee( pEnt->WorldSpaceCenter() ) )
    {
        pVision->UpdateLastSeen();
        return true;
    }

    return false;
}

bool NPCR::CBaseSenses::CanSee( const Vector& vecPos ) const
{
    CBaseCombatCharacter* pChar = GetOuter();

    Vector fwd;
    Vector start = pChar->EyePosition();
    Vector dir = vecPos - start;

    // Is the position within our field of view?
    AngleVectors( GetNPC()->GetAngles(), &fwd );
    Vector dir_noz = Vector( dir.x, dir.y, 0.0f ).Normalized();
    float dot = dir_noz.Dot( fwd );

    if ( dot < m_flFovCos )
    {
        return false;
    }


    return HasLOS( vecPos );
}

bool NPCR::CBaseSenses::HasLOS( const Vector& vecPos ) const
{
    CBaseCombatCharacter* pChar = GetOuter();

    Vector start = pChar->EyePosition();
    Vector dir = vecPos - start;
    float flToGoal = dir.NormalizeInPlace();

    trace_t tr;
    UTIL_TraceLine( start, start + dir * MIN( flToGoal, GetVisionDistance() ), MASK_VISIBLE, pChar, COLLISION_GROUP_NONE, &tr );
    
    return tr.fraction == 1.0f;
}

CBaseEntity* NPCR::CBaseSenses::GetClosestEntity() const
{
    float closest = FLT_MAX;
    const Vector vecMyPos = GetOuter()->WorldSpaceCenter();
    CBaseEntity* pClosest = nullptr;

    int len = m_vVisionEnts.Count();
    for ( int i = 0; i < len; i++ )
    {
        CBaseEntity* pEnt = m_vVisionEnts[i]->GetEntity();
        if ( !pEnt ) continue;


        float dist = pEnt->WorldSpaceCenter().DistToSqr( vecMyPos );
        if ( dist < closest )
        {
            closest = dist;
            pClosest = pEnt;
        }
    }

    return pClosest;
}
