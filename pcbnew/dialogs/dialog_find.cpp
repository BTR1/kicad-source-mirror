/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 1992-2012 Marco Mattila <marcom99@gmail.com>
 * Copyright (C) 1992-2012 Kicad Developers, see AUTHORS.txt for contributors.
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

#include <fctsys.h>
#include <gr_basic.h>
#include <class_drawpanel.h>
#include <confirm.h>
#include <kicad_string.h>
#include <wxPcbStruct.h>

#include <class_board.h>
#include <class_module.h>
#include <class_marker_pcb.h>

#include <pcbnew.h>
#include <pcbnew_id.h>
#include <protos.h>
#include <dialog_find_base.h>


class DIALOG_FIND : public DIALOG_FIND_BASE
{
public:
    DIALOG_FIND( PCB_BASE_FRAME* aParent );

private:
    PCB_BASE_FRAME* parent;

    int itemCount, markerCount;
    static wxString prevSearchString;
    static bool warpMouse;

    static wxPoint prevPosition;
    static wxSize prevSize;

    void onButtonFindItemClick( wxCommandEvent& event );
    void onButtonFindMarkerClick( wxCommandEvent& event );
    void onButtonCloseClick( wxCommandEvent& event );
    void onClose( wxCloseEvent& event );
};


// Initialize static member variables
wxPoint DIALOG_FIND::prevPosition( -1, -1 );
wxSize DIALOG_FIND::prevSize;
wxString DIALOG_FIND::prevSearchString;
bool DIALOG_FIND::warpMouse = true;


DIALOG_FIND::DIALOG_FIND( PCB_BASE_FRAME* aParent ) : DIALOG_FIND_BASE( aParent )
{
    parent = aParent;
    SetFocus();
    GetSizer()->SetSizeHints( this );

    m_SearchTextCtrl->AppendText( prevSearchString );
    m_NoMouseWarpCheckBox->SetValue( !warpMouse );

    itemCount = markerCount = 0;

    if( prevPosition.x != -1 )
        SetSize( prevPosition.x, prevPosition.y,
                 prevSize.x, prevSize.y );
    else
        Center();
}


void DIALOG_FIND::onButtonCloseClick( wxCommandEvent& aEvent )
{
    Close( true );
}


void DIALOG_FIND::onButtonFindItemClick( wxCommandEvent& aEvent )
{
    PCB_SCREEN* screen = (PCB_SCREEN*) ( parent->GetScreen() );
    wxPoint     pos;
    BOARD_ITEM* foundItem = 0;

    wxString searchString = m_SearchTextCtrl->GetValue();

    if( !searchString.IsSameAs( prevSearchString, false ) )
    {
        itemCount = 0;
    }
    prevSearchString = searchString;

    parent->GetCanvas()->GetViewStart( &screen->m_StartVisu.x, &screen->m_StartVisu.y );

    int count = 0;

    for( MODULE* module = parent->GetBoard()->m_Modules; module; module = module->Next() )
    {
        if( WildCompareString( searchString, module->GetReference().GetData(), false ) )
        {
            count++;

            if( count > itemCount )
            {
                foundItem = module;
                pos = module->GetPosition();
                itemCount++;
                break;
            }
        }

        if( WildCompareString( searchString, module->m_Value->m_Text.GetData(), false ) )
        {
            count++;

            if( count > itemCount )
            {
                foundItem = module;
                pos = module->m_Pos;
                itemCount++;
                break;
            }
        }
    }

    wxString msg;
    if( foundItem )
    {
        parent->SetCurItem( foundItem );
        msg.Printf( _( "<%s> found" ), GetChars( searchString ) );
        parent->SetStatusText( msg );

        parent->CursorGoto( pos, !m_NoMouseWarpCheckBox->IsChecked() );
    }
    else
    {
        parent->SetStatusText( wxEmptyString );
        msg.Printf( _( "<%s> not found" ), GetChars( searchString ) );
        DisplayError( this, msg, 10 );
        itemCount = 0;
    }
}


void DIALOG_FIND::onButtonFindMarkerClick( wxCommandEvent& aEvent )
{
    PCB_SCREEN* screen = (PCB_SCREEN*) ( parent->GetScreen() );
    wxPoint     pos;
    BOARD_ITEM* foundItem = 0;

    parent->GetCanvas()->GetViewStart( &screen->m_StartVisu.x, &screen->m_StartVisu.y );

    MARKER_PCB* marker = parent->GetBoard()->GetMARKER( markerCount++ );

    if( marker )
    {
        foundItem = marker;
        pos = marker->GetPosition();
    }

    wxString msg;
    if( foundItem )
    {
        parent->SetCurItem( foundItem );
        msg = _( "Marker found" );
        parent->SetStatusText( msg );

        parent->CursorGoto( pos, !m_NoMouseWarpCheckBox->IsChecked() );
    }
    else
    {
        parent->SetStatusText( wxEmptyString );
        msg = _( "No marker found" );
        DisplayError( this, msg, 10 );
        markerCount = 0;
    }
}


void DIALOG_FIND::onClose( wxCloseEvent& aEvent )
{
    prevPosition = GetPosition();
    prevSize = GetSize();
    warpMouse = !m_NoMouseWarpCheckBox->IsChecked();

    EndModal( 1 );
}


void PCB_EDIT_FRAME::InstallFindFrame()
{
    DIALOG_FIND dlg( this );
    dlg.ShowModal();
}
