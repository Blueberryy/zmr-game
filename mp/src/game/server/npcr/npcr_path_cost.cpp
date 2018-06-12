#include "cbase.h"

#include "npcr_path_cost.h"


// NAV path cost
float NPCR::CPathCostGroundOnly::operator()( CNavArea* area, CNavArea* fromArea, const CNavLadder* ladder, const CFuncElevator* elevator, float length ) const
{
    // This is the first area, initial cost is 0.
    if ( !fromArea )
    {
        return 0.0f;
    }

    // No ladders or elevators for now.
    if ( ladder || elevator )
        return PATHCOST_INVALID;


    // ZMRTODO: The updates are very slow. Have to try updating the block status here?
    if ( area->IsBlocked( TEAM_ANY ) )
    {
        //area->UpdateBlocked( true, TEAM_ANY );

        //if ( area->IsBlocked( TEAM_ANY ) )
            //return -1.0f;
        return PATHCOST_INVALID;
    }
            
    // The step up is too high.
    float height = fromArea->ComputeAdjacentConnectionHeightChange( area );
    if ( height > GetMaxHeightChange() )
    {
        return PATHCOST_INVALID;
    }

    // ZMRTODO: Add something to check for height.
    /*if ( GetHullHeight() > 0.0f )
    {
        if ( area->HasAttributes( NAV_MESH_CROUCH ) )
            return -1.0f;

        // Ceiling is too low for us?
        trace_t tr;
        Vector vecStart = area->GetCenter() + Vector( 0.0f, 0.0f, 1.0f );
        Vector vecEnd = vecStart;
        vecEnd.z += GetHullHeight();
        UTIL_TraceLine( vecStart, vecEnd, MASK_NPCSOLID_BRUSHONLY, nullptr, COLLISION_GROUP_NONE, &tr );
        if ( tr.DidHit() )
        {
            area->SetAttributes( area->GetAttributes() | NAV_MESH_CROUCH );
            return -1.0f;
        }
    }*/


    // By default the length is calculated from the center.
    // This takes into account where we start and end.
    // It gets very noticeable when the areas are large.
    if ( fromArea == m_pStartArea )
    {
        Vector pos;
        area->GetClosestPointOnArea( m_vecStart, &pos );

        length = m_vecStart.DistTo( pos );
    }

    if ( area == m_pGoalArea )
    {
        Vector pos;
        fromArea->GetClosestPointOnArea( m_vecEnd, &pos );

        length += m_vecEnd.DistTo( pos );
    }

    float cost = length + fromArea->GetCostSoFar();

    return cost;
}

// AI Graph path cost
float NPCR::CPathCostGroundOnly::operator()( const Vector& vecNodePos, const Vector& vecTestPos, int fCapBitMask ) const
{
    // This path isn't possible for our hull / it's disabled.
    if ( !fCapBitMask )
        return PATHCOST_INVALID;


    Vector dir = vecTestPos - vecNodePos;




    float dist = (vecNodePos - vecTestPos).Length();


    if ( fCapBitMask & bits_CAP_MOVE_JUMP )
    {
        // The step up is too high.
        if ( dir.z > GetMaxHeightChange() )
            return PATHCOST_INVALID;

        dist *= 2.0f;
    }


    return dist;
}
