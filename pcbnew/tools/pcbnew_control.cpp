/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2014 CERN
 * @author Maciej Suminski <maciej.suminski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "pcbnew_control.h"
#include "common_actions.h"
#include "selection_tool.h"
#include "picker_tool.h"

#include <pcbnew_id.h>
#include <wxPcbStruct.h>
#include <class_board.h>
#include <class_module.h>
#include <class_track.h>
#include <class_zone.h>
#include <class_draw_panel_gal.h>
#include <class_pcb_screen.h>
#include <confirm.h>
#include <hotkeys_basic.h>

#include <tool/tool_manager.h>
#include <gal/graphics_abstraction_layer.h>
#include <view/view_controls.h>
#include <pcb_painter.h>
#include <origin_viewitem.h>

#include <boost/bind.hpp>


PCBNEW_CONTROL::PCBNEW_CONTROL() :
    TOOL_INTERACTIVE( "pcbnew.Control" ), m_frame( NULL )
{
    m_gridOrigin = new KIGFX::ORIGIN_VIEWITEM();
}


PCBNEW_CONTROL::~PCBNEW_CONTROL()
{
    delete m_gridOrigin;
}


void PCBNEW_CONTROL::Reset( RESET_REASON aReason )
{
    m_frame = getEditFrame<PCB_BASE_FRAME>();

    if( aReason == MODEL_RELOAD || aReason == GAL_SWITCH )
    {
        m_gridOrigin->SetPosition( getModel<BOARD>()->GetGridOrigin() );
        getView()->Remove( m_gridOrigin );
        getView()->Add( m_gridOrigin );
    }
}


int PCBNEW_CONTROL::ZoomInOut( const TOOL_EVENT& aEvent )
{
    KIGFX::VIEW* view = m_frame->GetGalCanvas()->GetView();
    double zoomScale = 1.0;

    if( aEvent.IsAction( &COMMON_ACTIONS::zoomIn ) )
        zoomScale = 1.3;
    else if( aEvent.IsAction( &COMMON_ACTIONS::zoomOut ) )
        zoomScale = 0.7;

    view->SetScale( view->GetScale() * zoomScale, getViewControls()->GetCursorPosition() );

    return 0;
}


int PCBNEW_CONTROL::ZoomInOutCenter( const TOOL_EVENT& aEvent )
{
    KIGFX::VIEW* view = m_frame->GetGalCanvas()->GetView();
    double zoomScale = 1.0;

    if( aEvent.IsAction( &COMMON_ACTIONS::zoomInCenter ) )
        zoomScale = 1.3;
    else if( aEvent.IsAction( &COMMON_ACTIONS::zoomOutCenter ) )
        zoomScale = 0.7;

    view->SetScale( view->GetScale() * zoomScale );

    return 0;
}


int PCBNEW_CONTROL::ZoomCenter( const TOOL_EVENT& aEvent )
{
    KIGFX::VIEW* view = m_frame->GetGalCanvas()->GetView();
    view->SetCenter( getViewControls()->GetCursorPosition() );

    return 0;
}


int PCBNEW_CONTROL::ZoomFitScreen( const TOOL_EVENT& aEvent )
{
    KIGFX::VIEW* view = m_frame->GetGalCanvas()->GetView();
    KIGFX::GAL* gal = m_frame->GetGalCanvas()->GetGAL();
    BOARD* board = getModel<BOARD>();
    board->ComputeBoundingBox();
    BOX2I boardBBox = board->ViewBBox();
    VECTOR2I screenSize = gal->GetScreenPixelSize();

    if( boardBBox.GetSize().x == 0 || boardBBox.GetSize().y == 0 )
    {
        // Empty view
        view->SetCenter( view->ToWorld( VECTOR2D( screenSize.x / 2, screenSize.y / 2 ) ) );
        view->SetScale( 17.0 );
    }
    else
    {
        // Autozoom to board
        double iuPerX = screenSize.x ? boardBBox.GetWidth() / screenSize.x : 1.0;
        double iuPerY = screenSize.y ? boardBBox.GetHeight() / screenSize.y : 1.0;

        double bestZoom = std::max( iuPerX, iuPerY );
        double zoomFactor = gal->GetWorldScale() / gal->GetZoomFactor();
        double zoom = 1.0 / ( zoomFactor * bestZoom );

        view->SetCenter( boardBBox.Centre() );
        view->SetScale( zoom );
    }

    return 0;
}


int PCBNEW_CONTROL::ZoomPreset( const TOOL_EVENT& aEvent )
{
    unsigned int idx = aEvent.Parameter<long>();
    std::vector<double>& zoomList = m_frame->GetScreen()->m_ZoomList;
    KIGFX::VIEW* view = m_frame->GetGalCanvas()->GetView();
    KIGFX::GAL* gal = m_frame->GetGalCanvas()->GetGAL();

    m_frame->SetPresetZoom( idx );

    if( idx == 0 )      // Zoom Auto
    {
        return ZoomFitScreen( aEvent );
    }
    else if( idx >= zoomList.size() )
    {
        assert( false );
        return 0;
    }

    double selectedZoom = zoomList[idx];
    double zoomFactor = gal->GetWorldScale() / gal->GetZoomFactor();
    view->SetScale( 1.0 / ( zoomFactor * selectedZoom ) );

    return 0;
}


int PCBNEW_CONTROL::TrackDisplayMode( const TOOL_EVENT& aEvent )
{
    KIGFX::PCB_PAINTER* painter =
            static_cast<KIGFX::PCB_PAINTER*>( m_frame->GetGalCanvas()->GetView()->GetPainter() );
    KIGFX::PCB_RENDER_SETTINGS* settings =
            static_cast<KIGFX::PCB_RENDER_SETTINGS*>( painter->GetSettings() );

    // Apply new display options to the GAL canvas
    DISPLAY_OPTIONS* displ_opts = (DISPLAY_OPTIONS*)m_frame->GetDisplayOptions();
    displ_opts->m_DisplayPcbTrackFill = !displ_opts->m_DisplayPcbTrackFill;
    settings->LoadDisplayOptions( displ_opts );

    for( TRACK* track = getModel<BOARD>()->m_Track; track; track = track->Next() )
    {
        if( track->Type() == PCB_TRACE_T )
            track->ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
    }

    m_frame->GetGalCanvas()->Refresh();

    return 0;
}


int PCBNEW_CONTROL::PadDisplayMode( const TOOL_EVENT& aEvent )
{
    KIGFX::PCB_PAINTER* painter =
            static_cast<KIGFX::PCB_PAINTER*>( m_frame->GetGalCanvas()->GetView()->GetPainter() );
    KIGFX::PCB_RENDER_SETTINGS* settings =
            static_cast<KIGFX::PCB_RENDER_SETTINGS*>( painter->GetSettings() );
    DISPLAY_OPTIONS* displ_opts = (DISPLAY_OPTIONS*)m_frame->GetDisplayOptions();

    // Apply new display options to the GAL canvas
    displ_opts->m_DisplayPadFill = !displ_opts->m_DisplayPadFill;
    settings->LoadDisplayOptions( displ_opts );

    for( MODULE* module = getModel<BOARD>()->m_Modules; module; module = module->Next() )
    {
        for( D_PAD* pad = module->Pads(); pad; pad = pad->Next() )
            pad->ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
    }

    m_frame->GetGalCanvas()->Refresh();

    return 0;
}


int PCBNEW_CONTROL::ViaDisplayMode( const TOOL_EVENT& aEvent )
{
    KIGFX::PCB_PAINTER* painter =
            static_cast<KIGFX::PCB_PAINTER*>( m_frame->GetGalCanvas()->GetView()->GetPainter() );
    KIGFX::PCB_RENDER_SETTINGS* settings =
            static_cast<KIGFX::PCB_RENDER_SETTINGS*>( painter->GetSettings() );
    DISPLAY_OPTIONS* displ_opts = (DISPLAY_OPTIONS*)m_frame->GetDisplayOptions();

    // Apply new display options to the GAL canvas
    displ_opts->m_DisplayViaFill = !displ_opts->m_DisplayViaFill;
    settings->LoadDisplayOptions( displ_opts );

    for( TRACK* track = getModel<BOARD>()->m_Track; track; track = track->Next() )
    {
        if( track->Type() == PCB_VIA_T )
            track->ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
    }

    m_frame->GetGalCanvas()->Refresh();

    return 0;
}


int PCBNEW_CONTROL::ZoneDisplayMode( const TOOL_EVENT& aEvent )
{
    KIGFX::PCB_PAINTER* painter =
            static_cast<KIGFX::PCB_PAINTER*>( m_frame->GetGalCanvas()->GetView()->GetPainter() );
    KIGFX::PCB_RENDER_SETTINGS* settings =
            static_cast<KIGFX::PCB_RENDER_SETTINGS*>( painter->GetSettings() );
    DISPLAY_OPTIONS* displ_opts = (DISPLAY_OPTIONS*)m_frame->GetDisplayOptions();

    // Apply new display options to the GAL canvas
    if( aEvent.IsAction( &COMMON_ACTIONS::zoneDisplayEnable ) )
        displ_opts->m_DisplayZonesMode = 0;
    else if( aEvent.IsAction( &COMMON_ACTIONS::zoneDisplayDisable ) )
        displ_opts->m_DisplayZonesMode = 1;
    else if( aEvent.IsAction( &COMMON_ACTIONS::zoneDisplayOutlines ) )
        displ_opts->m_DisplayZonesMode = 2;
    else
        assert( false );

    settings->LoadDisplayOptions( displ_opts );

    BOARD* board = getModel<BOARD>();
    for( int i = 0; i < board->GetAreaCount(); ++i )
        board->GetArea( i )->ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );

    m_frame->GetGalCanvas()->Refresh();

    return 0;
}


int PCBNEW_CONTROL::HighContrastMode( const TOOL_EVENT& aEvent )
{
    KIGFX::PCB_PAINTER* painter =
            static_cast<KIGFX::PCB_PAINTER*>( m_frame->GetGalCanvas()->GetView()->GetPainter() );
    KIGFX::PCB_RENDER_SETTINGS* settings =
            static_cast<KIGFX::PCB_RENDER_SETTINGS*> ( painter->GetSettings() );
    DISPLAY_OPTIONS* displ_opts = (DISPLAY_OPTIONS*)m_frame->GetDisplayOptions();

    displ_opts->m_ContrastModeDisplay = !displ_opts->m_ContrastModeDisplay;
    settings->LoadDisplayOptions( displ_opts );
    m_frame->GetGalCanvas()->SetHighContrastLayer( m_frame->GetActiveLayer() );

    return 0;
}


int PCBNEW_CONTROL::HighContrastInc( const TOOL_EVENT& aEvent )
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    return 0;
}


int PCBNEW_CONTROL::HighContrastDec( const TOOL_EVENT& aEvent )
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    return 0;
}


// Layer control
int PCBNEW_CONTROL::LayerSwitch( const TOOL_EVENT& aEvent )
{
    m_frame->SwitchLayer( NULL, (LAYER_ID) aEvent.Parameter<long>() );

    return 0;
}


int PCBNEW_CONTROL::LayerNext( const TOOL_EVENT& aEvent )
{
    PCB_BASE_FRAME* editFrame = m_frame;
    LAYER_NUM layer = editFrame->GetActiveLayer();

    if( layer < F_Cu || layer > B_Cu )
        return 0;

    int layerCount = getModel<BOARD>()->GetCopperLayerCount();

    if( layer == layerCount - 2 || layerCount < 2 )
        layer = B_Cu;
    else if( layer == B_Cu )
        layer = F_Cu;
    else
        ++layer;

    assert( IsCopperLayer( layer ) );
    editFrame->SwitchLayer( NULL, ToLAYER_ID( layer ) );

    return 0;
}


int PCBNEW_CONTROL::LayerPrev( const TOOL_EVENT& aEvent )
{
    PCB_BASE_FRAME* editFrame = m_frame;
    LAYER_NUM layer = editFrame->GetActiveLayer();

    if( layer < F_Cu || layer > B_Cu )
        return 0;

    int layerCount = getModel<BOARD>()->GetCopperLayerCount();

    if( layer == F_Cu || layerCount < 2 )
        layer = B_Cu;
    else if( layer == B_Cu )
        layer = layerCount - 2;
    else
        --layer;

    assert( IsCopperLayer( layer ) );
    editFrame->SwitchLayer( NULL, ToLAYER_ID( layer ) );

    return 0;
}


int PCBNEW_CONTROL::LayerToggle( const TOOL_EVENT& aEvent )
{
    LAYER_NUM currentLayer = m_frame->GetActiveLayer();
    PCB_SCREEN* screen = m_frame->GetScreen();

    if( currentLayer == screen->m_Route_Layer_TOP )
        m_frame->SwitchLayer( NULL, screen->m_Route_Layer_BOTTOM );
    else
        m_frame->SwitchLayer( NULL, screen->m_Route_Layer_TOP );

    return 0;
}


int PCBNEW_CONTROL::LayerAlphaInc( const TOOL_EVENT& aEvent )
{
    KIGFX::PCB_PAINTER* painter =
            static_cast<KIGFX::PCB_PAINTER*>( m_frame->GetGalCanvas()->GetView()->GetPainter() );
    KIGFX::PCB_RENDER_SETTINGS* settings =
            static_cast<KIGFX::PCB_RENDER_SETTINGS*> ( painter->GetSettings() );

    LAYER_NUM currentLayer = m_frame->GetActiveLayer();
    KIGFX::COLOR4D currentColor = settings->GetLayerColor( currentLayer );

    if( currentColor.a <= 0.95 )
    {
        currentColor.a += 0.05;
        settings->SetLayerColor( currentLayer, currentColor );
        m_frame->GetGalCanvas()->GetView()->UpdateLayerColor( currentLayer );
    }

    return 0;
}


int PCBNEW_CONTROL::LayerAlphaDec( const TOOL_EVENT& aEvent )
{
    KIGFX::PCB_PAINTER* painter =
            static_cast<KIGFX::PCB_PAINTER*>( m_frame->GetGalCanvas()->GetView()->GetPainter() );
    KIGFX::PCB_RENDER_SETTINGS* settings =
            static_cast<KIGFX::PCB_RENDER_SETTINGS*> ( painter->GetSettings() );

    LAYER_NUM currentLayer = m_frame->GetActiveLayer();
    KIGFX::COLOR4D currentColor = settings->GetLayerColor( currentLayer );

    if( currentColor.a >= 0.05 )
    {
        currentColor.a -= 0.05;
        settings->SetLayerColor( currentLayer, currentColor );
        m_frame->GetGalCanvas()->GetView()->UpdateLayerColor( currentLayer );
    }

    return 0;
}


// Grid control
int PCBNEW_CONTROL::GridFast1( const TOOL_EVENT& aEvent )
{
    m_frame->SetFastGrid1();
    updateGrid();

    return 0;
}


int PCBNEW_CONTROL::GridFast2( const TOOL_EVENT& aEvent )
{
    m_frame->SetFastGrid2();
    updateGrid();

    return 0;
}


int PCBNEW_CONTROL::GridNext( const TOOL_EVENT& aEvent )
{
    m_frame->SetNextGrid();
    updateGrid();

    return 0;
}


int PCBNEW_CONTROL::GridPrev( const TOOL_EVENT& aEvent )
{
    m_frame->SetPrevGrid();
    updateGrid();

    return 0;
}


static bool setOrigin( KIGFX::VIEW* aView, PCB_BASE_FRAME* aFrame,
                       KIGFX::ORIGIN_VIEWITEM* aItem, const VECTOR2D& aPoint )
{
    aFrame->SetGridOrigin( wxPoint( aPoint.x, aPoint.y ) );
    aView->GetGAL()->SetGridOrigin( aPoint );
    aItem->SetPosition( aPoint );
    aView->MarkDirty();

    return true;
}


int PCBNEW_CONTROL::GridSetOrigin( const TOOL_EVENT& aEvent )
{
    PICKER_TOOL* picker = m_toolMgr->GetTool<PICKER_TOOL>();
    assert( picker );

    // TODO it will not check the toolbar button in module editor, as it uses a different ID..
    m_frame->SetToolID( ID_PCB_PLACE_GRID_COORD_BUTT, wxCURSOR_PENCIL, _( "Adjust grid origin" ) );
    picker->SetClickHandler( boost::bind( setOrigin, getView(), m_frame, m_gridOrigin, _1 ) );
    picker->Activate();

    return 0;
}


int PCBNEW_CONTROL::GridPreset( const TOOL_EVENT& aEvent )
{
    long idx = aEvent.Parameter<long>();

    m_frame->SetPresetGrid( idx );
    updateGrid();

    return 0;
}


// Miscellaneous
int PCBNEW_CONTROL::ResetCoords( const TOOL_EVENT& aEvent )
{
    VECTOR2I cursorPos = getViewControls()->GetCursorPosition();

    m_frame->GetScreen()->m_O_Curseur = wxPoint( cursorPos.x, cursorPos.y );
    m_frame->UpdateStatusBar();

    return 0;
}


int PCBNEW_CONTROL::SwitchCursor( const TOOL_EVENT& aEvent )
{
    const unsigned int BIG_CURSOR = 4000;
    const unsigned int SMALL_CURSOR = 80;

    KIGFX::GAL* gal = getEditFrame<PCB_BASE_FRAME>()->GetGalCanvas()->GetGAL();

    if( gal->GetCursorSize() == BIG_CURSOR )
        gal->SetCursorSize( SMALL_CURSOR );
    else
        gal->SetCursorSize( BIG_CURSOR );

    return 0;
}


int PCBNEW_CONTROL::SwitchUnits( const TOOL_EVENT& aEvent )
{
    // TODO should not it be refactored to pcb_frame member function?
    wxCommandEvent evt( wxEVT_COMMAND_MENU_SELECTED );

    if( g_UserUnit == INCHES )
        evt.SetId( ID_TB_OPTIONS_SELECT_UNIT_MM );
    else
        evt.SetId( ID_TB_OPTIONS_SELECT_UNIT_INCH );

    m_frame->ProcessEvent( evt );

    return 0;
}


static bool deleteItem( TOOL_MANAGER* aToolMgr, const VECTOR2D& aPosition )
{
    SELECTION_TOOL* selectionTool = aToolMgr->GetTool<SELECTION_TOOL>();
    assert( selectionTool );

    aToolMgr->RunAction( COMMON_ACTIONS::selectionClear, true );
    aToolMgr->RunAction( COMMON_ACTIONS::selectionCursor, true );
    selectionTool->SanitizeSelection();

    if( selectionTool->GetSelection().Empty() )
        return true;

    if( IsOK( aToolMgr->GetEditFrame(), _( "Are you sure you want to delete item?" ) ) )
        aToolMgr->RunAction( COMMON_ACTIONS::remove, true );

    aToolMgr->RunAction( COMMON_ACTIONS::selectionClear, true );

    return true;
}


int PCBNEW_CONTROL::DeleteItemCursor( const TOOL_EVENT& aEvent )
{
    PICKER_TOOL* picker = m_toolMgr->GetTool<PICKER_TOOL>();
    assert( picker );

    // TODO it will not check the toolbar button in the module editor, as it uses a different ID..
    m_frame->SetToolID( ID_PCB_DELETE_ITEM_BUTT, wxCURSOR_PENCIL, _( "Delete item" ) );
    picker->SetSnapping( false );
    picker->SetClickHandler( boost::bind( deleteItem, m_toolMgr, _1 ) );
    picker->Activate();

    return 0;
}


int PCBNEW_CONTROL::ShowHelp( const TOOL_EVENT& aEvent )
{
    DisplayHotkeyList( m_frame, m_frame->GetHotkeyConfig() );

    return 0;
}


int PCBNEW_CONTROL::ToBeDone( const TOOL_EVENT& aEvent )
{
    DisplayInfoMessage( m_frame, _( "Not available in OpenGL/Cairo canvases." ) );

    return 0;
}


void PCBNEW_CONTROL::SetTransitions()
{
    // View controls
    Go( &PCBNEW_CONTROL::ZoomInOut,          COMMON_ACTIONS::zoomIn.MakeEvent() );
    Go( &PCBNEW_CONTROL::ZoomInOut,          COMMON_ACTIONS::zoomOut.MakeEvent() );
    Go( &PCBNEW_CONTROL::ZoomInOutCenter,    COMMON_ACTIONS::zoomInCenter.MakeEvent() );
    Go( &PCBNEW_CONTROL::ZoomInOutCenter,    COMMON_ACTIONS::zoomOutCenter.MakeEvent() );
    Go( &PCBNEW_CONTROL::ZoomCenter,         COMMON_ACTIONS::zoomCenter.MakeEvent() );
    Go( &PCBNEW_CONTROL::ZoomFitScreen,      COMMON_ACTIONS::zoomFitScreen.MakeEvent() );
    Go( &PCBNEW_CONTROL::ZoomPreset,         COMMON_ACTIONS::zoomPreset.MakeEvent() );

    // Display modes
    Go( &PCBNEW_CONTROL::TrackDisplayMode,   COMMON_ACTIONS::trackDisplayMode.MakeEvent() );
    Go( &PCBNEW_CONTROL::PadDisplayMode,     COMMON_ACTIONS::padDisplayMode.MakeEvent() );
    Go( &PCBNEW_CONTROL::ViaDisplayMode,     COMMON_ACTIONS::viaDisplayMode.MakeEvent() );
    Go( &PCBNEW_CONTROL::ZoneDisplayMode,    COMMON_ACTIONS::zoneDisplayEnable.MakeEvent() );
    Go( &PCBNEW_CONTROL::ZoneDisplayMode,    COMMON_ACTIONS::zoneDisplayDisable.MakeEvent() );
    Go( &PCBNEW_CONTROL::ZoneDisplayMode,    COMMON_ACTIONS::zoneDisplayOutlines.MakeEvent() );
    Go( &PCBNEW_CONTROL::HighContrastMode,   COMMON_ACTIONS::highContrastMode.MakeEvent() );
    Go( &PCBNEW_CONTROL::HighContrastInc,    COMMON_ACTIONS::highContrastInc.MakeEvent() );
    Go( &PCBNEW_CONTROL::HighContrastDec,    COMMON_ACTIONS::highContrastDec.MakeEvent() );

    // Layer control
    Go( &PCBNEW_CONTROL::LayerSwitch,        COMMON_ACTIONS::layerTop.MakeEvent() );
    Go( &PCBNEW_CONTROL::LayerSwitch,        COMMON_ACTIONS::layerInner1.MakeEvent() );
    Go( &PCBNEW_CONTROL::LayerSwitch,        COMMON_ACTIONS::layerInner2.MakeEvent() );
    Go( &PCBNEW_CONTROL::LayerSwitch,        COMMON_ACTIONS::layerInner3.MakeEvent() );
    Go( &PCBNEW_CONTROL::LayerSwitch,        COMMON_ACTIONS::layerInner4.MakeEvent() );
    Go( &PCBNEW_CONTROL::LayerSwitch,        COMMON_ACTIONS::layerInner5.MakeEvent() );
    Go( &PCBNEW_CONTROL::LayerSwitch,        COMMON_ACTIONS::layerInner6.MakeEvent() );
    Go( &PCBNEW_CONTROL::LayerSwitch,        COMMON_ACTIONS::layerBottom.MakeEvent() );
    Go( &PCBNEW_CONTROL::LayerNext,          COMMON_ACTIONS::layerNext.MakeEvent() );
    Go( &PCBNEW_CONTROL::LayerPrev,          COMMON_ACTIONS::layerPrev.MakeEvent() );
    Go( &PCBNEW_CONTROL::LayerToggle,        COMMON_ACTIONS::layerToggle.MakeEvent() );
    Go( &PCBNEW_CONTROL::LayerAlphaInc,      COMMON_ACTIONS::layerAlphaInc.MakeEvent() );
    Go( &PCBNEW_CONTROL::LayerAlphaDec,      COMMON_ACTIONS::layerAlphaDec.MakeEvent() );

    // Grid control
    Go( &PCBNEW_CONTROL::GridFast1,          COMMON_ACTIONS::gridFast1.MakeEvent() );
    Go( &PCBNEW_CONTROL::GridFast2,          COMMON_ACTIONS::gridFast2.MakeEvent() );
    Go( &PCBNEW_CONTROL::GridNext,           COMMON_ACTIONS::gridNext.MakeEvent() );
    Go( &PCBNEW_CONTROL::GridPrev,           COMMON_ACTIONS::gridPrev.MakeEvent() );
    Go( &PCBNEW_CONTROL::GridSetOrigin,      COMMON_ACTIONS::gridSetOrigin.MakeEvent() );
    Go( &PCBNEW_CONTROL::GridPreset,         COMMON_ACTIONS::gridPreset.MakeEvent() );

    // Miscellaneous
    Go( &PCBNEW_CONTROL::ResetCoords,        COMMON_ACTIONS::resetCoords.MakeEvent() );
    Go( &PCBNEW_CONTROL::SwitchCursor,       COMMON_ACTIONS::switchCursor.MakeEvent() );
    Go( &PCBNEW_CONTROL::SwitchUnits,        COMMON_ACTIONS::switchUnits.MakeEvent() );
    Go( &PCBNEW_CONTROL::DeleteItemCursor,   COMMON_ACTIONS::deleteItemCursor.MakeEvent() );
    Go( &PCBNEW_CONTROL::ShowHelp,           COMMON_ACTIONS::showHelp.MakeEvent() );
    Go( &PCBNEW_CONTROL::ToBeDone,           COMMON_ACTIONS::toBeDone.MakeEvent() );
}


void PCBNEW_CONTROL::updateGrid()
{
    BASE_SCREEN* screen = m_frame->GetScreen();
    //GRID_TYPE grid = screen->GetGrid( idx );
    getView()->GetGAL()->SetGridSize( VECTOR2D( screen->GetGridSize() ) );
    getView()->MarkTargetDirty( KIGFX::TARGET_NONCACHED );
}
