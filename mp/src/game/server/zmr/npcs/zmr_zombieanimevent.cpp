#include "cbase.h"

#include "Multiplayer/multiplayer_animstate.h"
#include "zmr/npcs/zmr_zombieanimstate.h"

#include "zmr_zombieanimevent.h"


static CZMTEZombieAnimEvent g_ZMTEZombieAnimEvent( "ZombieAnimEvent" );


IMPLEMENT_SERVERCLASS_ST_NOBASE( CZMTEZombieAnimEvent, DT_ZM_TEZombieAnimEvent )
    SendPropEHandle( SENDINFO( m_hZombie ) ),
    SendPropInt( SENDINFO( m_iEvent ), Q_log2( ZOMBIEANIMEVENT_MAX ) + 1, SPROP_UNSIGNED ),
    //SendPropInt( SENDINFO( m_nData ), 32 )
END_SEND_TABLE()

void TE_ZombieAnimEvent( CZMBaseZombie* pZombie, ZMZombieAnimEvent_t anim, int nData )
{
    CPVSFilter filter( pZombie->WorldSpaceCenter() );


    filter.UsePredictionRules();
    
    g_ZMTEZombieAnimEvent.m_hZombie = pZombie;
    g_ZMTEZombieAnimEvent.m_iEvent = anim;
    //g_ZMTEZombieAnimEvent.m_nData = nData;
    g_ZMTEZombieAnimEvent.Create( filter, 0 );
}
